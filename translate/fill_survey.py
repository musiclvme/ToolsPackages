#!/usr/bin/env python3
"""Fill empty English vendor-survey replies from the Chinese QMS answers.

Already-filled English content is never overwritten.
"""
from __future__ import annotations

from copy import deepcopy
from pathlib import Path

from docx import Document
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.text.paragraph import Paragraph

SRC = Path("/workspace/translate/Vendor_Survey_Laboratory_Cerba_Research_SOBC_V1.docx")
DST = Path(
    "/workspace/translate/Vendor_Survey_Laboratory_Cerba_Research_SOBC_V1_filled.docx"
)

PLACEHOLDERS = {
    "",
    "please explain",
    "please explain.",
    "please briefly describe the process.",
    "please briefly describe the process",
    "how long are samples retained following analysis?",
    "if yes; please explain",
    "please explain (+ how often)",
    "please provide information:",
    "please provide information",
    "please provide information: ",
}


def unique_cells(row):
    seen, out = [], []
    for cell in row.cells:
        ident = id(cell._tc)
        if ident in seen:
            continue
        seen.append(ident)
        out.append(cell)
    return out


def cell_text(cell) -> str:
    return "\n".join(p.text for p in cell.paragraphs).strip()


def is_placeholder(text: str) -> bool:
    t = " ".join(text.strip().split()).lower().rstrip(".")
    return t in PLACEHOLDERS


def make_sym_run(char: str = "00FE"):
    r = OxmlElement("w:r")
    rPr = OxmlElement("w:rPr")
    rFonts = OxmlElement("w:rFonts")
    rFonts.set(qn("w:ascii"), "Wingdings")
    rFonts.set(qn("w:hAnsi"), "Wingdings")
    rPr.append(rFonts)
    sz = OxmlElement("w:sz")
    sz.set(qn("w:val"), "22")
    rPr.append(sz)
    r.append(rPr)
    sym = OxmlElement("w:sym")
    sym.set(qn("w:font"), "Wingdings")
    sym.set(qn("w:char"), char)
    r.append(sym)
    return r


def append_check(paragraph: Paragraph, char: str = "00FE") -> None:
    paragraph._p.append(make_sym_run(char))


def prepend_check(paragraph: Paragraph, char: str = "00FE") -> None:
    paragraph._p.insert(0, make_sym_run(char))


def clear_runs(paragraph: Paragraph) -> None:
    for child in list(paragraph._p):
        tag = child.tag.split("}")[-1]
        if tag in ("r", "hyperlink", "sdt"):
            paragraph._p.remove(child)


def set_paragraph_text(paragraph: Paragraph, text: str, bold: bool = False) -> None:
    clear_runs(paragraph)
    run = paragraph.add_run(text)
    run.font.name = "Arial"
    r = run._r
    rPr = r.get_or_add_rPr()
    rFonts = rPr.find(qn("w:rFonts"))
    if rFonts is None:
        rFonts = OxmlElement("w:rFonts")
        rPr.append(rFonts)
    rFonts.set(qn("w:ascii"), "Arial")
    rFonts.set(qn("w:hAnsi"), "Arial")
    rFonts.set(qn("w:cs"), "Arial")
    if bold:
        run.bold = True


def set_cell_text(cell, text: str, force: bool = False) -> bool:
    current = cell_text(cell)
    if not force and not is_placeholder(current):
        return False
    lines = text.split("\n")
    paras = cell.paragraphs
    if not paras:
        return False
    set_paragraph_text(paras[0], lines[0] if lines else "")
    # extra paragraphs: reuse if present, else clone first
    existing = list(paras)
    for i, line in enumerate(lines[1:], start=1):
        if i < len(existing):
            set_paragraph_text(existing[i], line)
        else:
            new_p = deepcopy(existing[0]._p)
            cell._tc.append(new_p)
            set_paragraph_text(Paragraph(new_p, cell), line)
    # clear leftover paragraphs
    leftover = cell.paragraphs[len(lines) :]
    for p in leftover:
        set_paragraph_text(p, "")
    return True


def mark_yna(row, choice: str) -> None:
    """choice in yes/no/na. Adds a Wingdings tick to an empty Yes/No/N/A cell."""
    cells = unique_cells(row)
    if len(cells) < 5:
        return
    idx = {"yes": 1, "no": 2, "na": 3}[choice]
    cell = cells[idx]
    # already has a check or meaningful text? skip (except stray whitespace)
    if cell._tc.findall(".//" + qn("w:sym")):
        return
    txt = cell_text(cell).strip()
    if txt and txt.lower() not in {"yes", "no", "n/a", "na"}:
        # e.g. accidental "d" — do not overwrite
        return
    p = cell.paragraphs[0] if cell.paragraphs else None
    if p is None:
        return
    append_check(p)


def check_sdt_paragraphs(cell, para_indices: list[int]) -> None:
    paras = cell.paragraphs
    for i in para_indices:
        if i >= len(paras):
            continue
        p = paras[i]
        for sdt in p._p.findall(qn("w:sdt")):
            for el in sdt.findall(".//" + qn("w14:checked")):
                el.set(qn("w14:val"), "1")
            for t_el in sdt.findall(".//" + qn("w:t")):
                if t_el.text is not None:
                    t_el.text = "☒"


def add_check_to_para(cell, para_index: int) -> None:
    paras = cell.paragraphs
    if para_index >= len(paras):
        return
    p = paras[para_index]
    if p._p.findall(".//" + qn("w:sym")):
        return
    # put check at start of paragraph
    p._p.insert(0, make_sym_run("00FE"))


def fill_comment(table, row_i: int, text: str, force: bool = False) -> bool:
    row = table.rows[row_i]
    cell = unique_cells(row)[-1]
    return set_cell_text(cell, text, force=force)


def mark_inline_yes(cell) -> None:
    """Insert a tick before the first 'Yes' run in a Yes/No cell."""
    if cell._tc.findall(".//" + qn("w:sym")):
        return
    for p in cell.paragraphs:
        for child in list(p._p):
            if child.tag == qn("w:r"):
                texts = "".join(t.text or "" for t in child.findall(qn("w:t")))
                if "Yes" in texts or texts.strip().startswith("Yes"):
                    child.addprevious(make_sym_run("00FE"))
                    return
        if "Yes" in p.text:
            prepend_check(p)
            return


# ---------------------------------------------------------------------------
# Translations (ISO 15189 / GCP / GCLP laboratory QMS wording)
# ---------------------------------------------------------------------------

T1_FUNCTION = "Quality Manager (Quality Responsible Person)"
T1_PHONE = "+86 13902559694"
T1_EMAIL = "linrong_liang@shbiochip.com"
T1_BUSINESS = (
    "Shanghai Outdo Clinic Co., Ltd. is a wholly-owned subsidiary of Shanghai Outdo "
    "Biotech Co., Ltd., established in 2016 and located at Building 15, Lane 908, "
    "Ziping Road, Zhoupu Town, Pudong New Area, Shanghai, China. Registered capital "
    "is RMB 45.81 million. The site occupies 1,858.94 m², of which 892.29 m² is "
    "laboratory space. The company is an independent legal entity and a for-profit "
    "joint-stock medical institution. It provides medical laboratory and pathology "
    "services in clinical microbiology, pathology and clinical cytomolecular "
    "diagnostics, and has established its quality management system in accordance "
    "with CNAS-CL02:2023 Medical laboratories — Requirements for quality and "
    "competence (ISO 15189:2022, IDT) and related accreditation criteria."
)
T1_HOW_LONG = "Since 2019."
T1_OTHER_EXPERTISE = (
    "Other, please specify: clinical pathology diagnostics (IHC & ISH); clinical "
    "bacterial culture (Helicobacter pylori culture and antimicrobial susceptibility "
    "testing); gut microbiome testing; cell culture and passaging; viral seed-stock "
    "preparation; viral titration; ELISA; TCID50."
)
T1_SUPPLEMENTAL = (
    "Services/testing relevant to Cerba Research include: RT-qPCR; sequencing; "
    "cell culture; turkey (avian) blood collection; virus culture; virus titration; "
    "TCID50; ELISA; EC50."
)
T1_TURNOVER = "RMB 80 million (last two years)."
T1_PARENT = (
    "Shanghai Outdo Biotech Co., Ltd.\n"
    "No. 151 Libing Road, Zhangjiang, Pudong New Area, Shanghai, China.\n"
    "Shanghai Outdo Biotech Co., Ltd. was established in December 2003 and is part "
    "of the Shanghai Biochip Co., Ltd. group. It is responsible for operation of the "
    "central platform, including the clinical-translation CBDTM paradigm. It is "
    "recognized as a national-level Specialized, Refined, Distinctive and Innovative "
    "“Little Giant” enterprise and a Shanghai key high-tech enterprise, operating "
    "under a dual-driver model with Outdo and the biochip platform.\n"
    "Shanghai Biochip Co., Ltd. was established in August 2001 by Shanghai Science "
    "and Technology Venture Capital (Group) Co., Ltd., Shanghai Zhangjiang (Group) "
    "Co., Ltd., relevant CAS Shanghai institutes, Shanghai Jiao Tong University and "
    "other universities, research institutes, hospitals and enterprises. With NDRC "
    "approval it constructs and operates the National Engineering Research Center "
    "for Biochip at Shanghai, one of the largest national engineering research "
    "centers in China’s biopharmaceutical field. The Center serves as the secretariat "
    "and chair organization of the National Technical Committee on Biobanking "
    "Standardization (SAC/TC559), the Biobank Branch of the China Medicinal "
    "Biotechnology Association, and the Tumor Biospecimen Integration Research "
    "Branch of the China Anti-Cancer Association."
)
T1_CUSTOMER = "N/A"
T1_DEBARMENT_NOTE = (
    "Yes. The laboratory confirms that none of its employees is listed on the US FDA "
    "debarment list."
)
T1_CERT_NOTE = "Yes. ISO 15189 accreditation (CNAS)."

T2_QMS_YES = "ISO 15189."
T2_FRAMEWORK = (
    "The QMS framework is established in accordance with CNAS-CL02:2023 Medical "
    "laboratories — Requirements for quality and competence (ISO 15189:2022, IDT) "
    "and CNAS-CL02-A001:2023 Application Requirements of the Accreditation Criteria "
    "for the Quality and Competence of Medical Laboratories. The documented system "
    "comprises: Quality Manual; procedures (SOPs); work instructions; and related "
    "controlled records/forms."
)
T2_QM = "Yes. A written Quality Manual is available (ODC-ZLSC Quality Manual)."
T2_QA_DEPT = "Yes. An independent Quality Department has been established."
T2_QA_RESP = (
    "1. QMS establishment and maintenance: participate in drafting, review, revision, "
    "issuance and retrieval of QMS documents (SOPs, Quality Manual, record templates, "
    "etc.); participate in management review and oversee continued effective operation "
    "of the system.\n"
    "2. Training support: assist in organizing quality training (GLP requirements, "
    "documentation/record practices, deviation management, etc.) and oversee "
    "implementation of personnel training and authorization.\n"
    "3. Manage nonconforming work, corrective actions, preventive actions and "
    "continual improvement."
)
T2_COVERED = (
    "Yes. Activities and processes are covered by approved procedures. Documented "
    "ODC-CX-33 Document Control Procedure and ODC-CX-34 Record Management Procedure "
    "are in place. Document revision follows a defined process. Corresponding approved "
    "procedures, SOPs or work instructions are designed to cover activities, with "
    "records retained as evidence of execution."
)
T2_SOP_ACCESS = (
    "Creation: drafted by the process-owner department based on applicable "
    "regulations, QMS requirements, industry practice and internal experience; "
    "reviewed by the department head and Quality Manager and approved by the "
    "Laboratory Director before release.\n"
    "Maintenance: reviewed at least annually, or promptly revised upon process "
    "change or regulatory update. Each document records revision history and the "
    "reason for change. Obsolete versions are archived and clearly identified to "
    "prevent unintended use.\n"
    "Access: current paper copies are signed/stamped and issued to departments; "
    "electronic copies are stored on a shared server/internal platform with "
    "role- and department-based access. Training is provided at onboarding and upon "
    "document update, with training records retained. A master list of controlled "
    "documents is maintained to ensure traceability."
)
T2_OBSOLETE = (
    "Paper copies: after a new version is released, the document administrator "
    "promptly retrieves all obsolete printouts from workstations and laboratories, "
    "stamps them “Obsolete”, archives them for the defined period and then destroys "
    "them. Electronic copies: all controlled documents are stored centrally on the "
    "server; after a new version is released the previous version is superseded and "
    "ordinary users cannot access or download it.\n"
    "Internal audits include workplace checks to confirm that no uncontrolled or "
    "obsolete documents remain at points of use."
)
T2_REVISION = (
    "Yes. Procedures are maintained under a documented revision policy in ODC-CX-33 "
    "Document Control Procedure:\n"
    "To revise a document, the applicant completes a Document Application Form for "
    "review by the original reviewer (or the Quality Manager if the original reviewer "
    "is unavailable). After Quality Manager approval, the revised draft is reviewed "
    "by the original reviewer and approved/released by the designated approver.\n"
    "To add a document, the applicant completes a Document Application Form; after "
    "approval the drafting–review–approval–release process is followed. The document "
    "administrator updates the Document Control List and the Document Issue and "
    "Retrieval Register.\n"
    "Handwritten alteration of QMS documents is not permitted. Changes shall be "
    "recorded on the revision page with the approver identified. When a document is "
    "reissued, Quality retrieves and destroys the previous version. After periodic "
    "review, the document administrator prepares the Annual Document Review Record "
    "for approval, then arranges revision."
)
T2_GDP = (
    "Good Documentation Practices are implemented under ODC-CX-33 Document Control "
    "Procedure, including:\n"
    "Document control status: controlled / uncontrolled / obsolete copies are stamped "
    "accordingly to prevent misuse.\n"
    "Unique identification: documents are numbered as Company abbreviation–category–"
    "serial number; record forms add “R + sequence”.\n"
    "Version control: version letters A/B/C… and revision numbers 0/1/2…; a version "
    "increment is required after nine revisions.\n"
    "Segregation of review and approval: designated reviewers and approvers sign at "
    "defined document levels.\n"
    "Defined interval between approval date and effective date.\n"
    "Periodic review before the annual internal audit, with revision as needed.\n"
    "Record-entry rules: blue/black ink; corrections by single-line strikethrough "
    "with signature (no correction fluid), consistent with ALCOA+ / GCP documentation "
    "expectations.\n"
    "Archive protection against mould, insects, rodents and fire; electronic and paper "
    "records are regarded as equivalent.\n"
    "Defined retention periods (e.g. laboratory raw data 6 years; tumour molecular "
    "testing records 20 years; clinical-trial records per customer/regulatory "
    "requirement).\n"
    "Controlled destruction: expiry destruction requires application/approval, a "
    "destruction record and confidential handling."
)

T3_SECURITY = (
    "Yes. Per ODC-CX-10 Facility and Environmental Management Procedure, the site is "
    "equipped with safety facilities and equipment, including emergency evacuation "
    "devices and alarm systems, emergency showers and eyewash stations, with periodic "
    "functional verification. Access to critical areas is restricted: laboratory staff "
    "enter by access card or fingerprint; other persons enter only with authorization. "
    "Smoking is prohibited throughout the premises. The safety administrator maintains "
    "fire-fighting equipment and checks its effectiveness regularly."
)
T3_OVERVIEW = (
    "Per ODC-CX-10 Facility and Environmental Management Procedure, the facility is "
    "purpose-designed with zoned areas separated by laboratory-grade fire-resistant "
    "boards/partitions of defined height and thickness.\n"
    "Per ODC-AQ-34 Company Risk Assessment and Risk Control SOP, the laboratory is "
    "operated at biosafety level 2 (BSL-2). Principal use is medical laboratory "
    "testing, including throat swabs, gastric mucosa and other specimen types.\n"
    "Per ODC-AQ-42 Pest Control SOP, pest-control coverage includes the laboratory "
    "perimeter, open areas and laboratory rooms.\n"
    "Total site area is 1,858.94 m², of which 892.29 m² is laboratory space "
    "(see company profile)."
)
T3_PEST = (
    "Yes. Pest control is defined in ODC-AQ-42 Pest Control SOP. The General "
    "Administration Department prepares the pest-control plan and oversees completion; "
    "routine in-house pest-control activities are performed by that department."
)
T3_PEST_INSP = (
    "Yes. Per ODC-AQ-42 Pest Control SOP, internal pest/rodent control is performed "
    "monthly. Glue-board locations are fixed, kept clean and cleared of carcasses and "
    "debris. Placement locations and quantities are documented on ODC-AQ-42-R01 "
    "Pest and Rodent Control Record."
)
T3_TEMP = (
    "Yes. Per ODC-CX-10 Facility and Environmental Management Procedure, testing areas "
    "with temperature or humidity requirements are equipped with thermometers/"
    "hygrometers. On-duty laboratory staff record temperature/humidity daily. "
    "Freezers/refrigerators used for reagents or samples, incubators and similar "
    "equipment have thermometers or probes. A designated person, assigned by the "
    "Laboratory Director, records temperatures and checks the monitoring system. "
    "Records are kept on ODC-CX-10-R01 Indoor Temperature and Humidity Record."
)
T3_OOS = (
    "Per ODC-CX-10 Facility and Environmental Management Procedure: when temperature "
    "or humidity is out of specification, prompt adjustment is required (HVAC for "
    "temperature; dehumidifier if humidity is high; humidifier if humidity is low). "
    "If specification cannot be restored, the departmental equipment administrator "
    "is notified to arrange maintenance; testing is stopped during repair and "
    "ODC-CX-29 Nonconformity Identification and Control Procedure is executed. For "
    "reagent/sample storage units, incubators and similar equipment, stored materials "
    "are transferred to qualified units during repair. Upon detecting an excursion, "
    "the Laboratory Director is notified immediately; the quality supervisor completes "
    "the Facility and Environment Out-of-Control and Handling Record. If result quality "
    "may be affected, testing is stopped and the nonconformity procedure is applied."
)
T3_GENERATOR = (
    "No dedicated back-up generator is installed. Dual-feed electrical supply is "
    "available (manual switchover, not automatic). Five UPS units are connected to "
    "critical equipment."
)
T3_GENERATOR_TEST = (
    "N/A for generators (none installed). Electrical installations are periodically "
    "maintained by qualified electrical personnel per the Electrical Safety Standard. "
    "UPS maintenance is documented on ODC-AQ-27-R01 UPS Maintenance Record and "
    "ODC-AQ-27-R02 UPS Maintenance Plan."
)
T3_BCP = (
    "Yes. Per ODC-CX-10 Facility and Environmental Management Procedure, emergency "
    "measures are defined for unexpected water or power interruption. A laboratory "
    "safety risk assessment is performed annually and is re-performed when facilities "
    "change. Per ODC-CX-32 Emergency Preparedness Procedure, the General Administration "
    "Department prepares the internal Emergency Drill Plan each January for approval "
    "by the Laboratory Director."
)
T3_BCP_TEST = (
    "Yes, drills are performed and documented: ODC-CX-32-R01 Emergency Drill Plan; "
    "ODC-CX-32-R02 Emergency Drill Record (materials, participants, timing, process "
    "and recording method are planned in advance; after the drill the form is "
    "completed and reviewed by the safety responsible person before archiving)."
)
T3_BCP_RISK = (
    "Yes. Business continuity / disaster-recovery arrangements are linked to a "
    "documented, risk-based approach: an annual laboratory safety risk assessment is "
    "performed, and re-assessment is required when facilities change, with reference "
    "to ODC-CX-08 Risk Management Procedure (ODC-CX-10). Emergency preparedness and "
    "drills are planned and recorded under ODC-CX-32."
)

T4_JD = (
    "Yes. Up-to-date job descriptions and CVs are maintained for all personnel under "
    "ODC-CX-09 Human Resource Management Procedure and are available upon request."
)
T4_GENERAL = (
    "Yes. A documented general training process is in place under ODC-CX-09 Human "
    "Resource Management Procedure."
)
T4_OVERALL = (
    "Per ODC-CX-09 Human Resource Management Procedure, training is delivered by "
    "lecture, reading/self-study, other methods (e.g. online meetings) and external "
    "training. The laboratory training plan is compiled, checked, approved and posted "
    "by the document administrator; training notices are issued in advance; attendance, "
    "materials and personnel training histories are maintained. After training, the "
    "trainer conducts assessment and submits training records. The safety responsible "
    "person periodically organizes safety training and emergency drills. Unplanned "
    "internal/external training requires stepwise approval; effectiveness evaluation "
    "is performed after external training."
)
T4_GDPR_NOTE = (
    " — confidentiality is managed under ODC-CX-04 Confidentiality Management Procedure"
)

T8_CM = (
    "Yes. Change control is implemented across multiple controlled procedures covering "
    "documents (ODC-CX-33 Document Control Procedure), examination procedures "
    "(ODC-CX-20 Selection, Verification and Validation of Examination Procedures), "
    "IT systems (ODC-CX-30 Computer Hardware and Software Management Procedure), "
    "personnel (ODC-CX-09 Human Resource Management Procedure) and reagents/suppliers "
    "(ODC-CX-14 Reagent and Consumable Management Procedure)."
)
T8_RISK = (
    "Yes, a risk-based approach is applied in practice, although not always labelled "
    "as such in every SOP. Examples: personnel/role changes require revision and "
    "approval of job descriptions; accreditation-related changes are notified to CNAS "
    "within 20 working days; retraining/reassessment is triggered by role change, "
    "return after >6 months’ absence, or policy/procedure/technology change "
    "(ODC-CX-09). Facility changes trigger re-assessment against ODC-CX-08 Risk "
    "Management Procedure (ODC-CX-10). After equipment repair, return from storage or "
    "relocation, verification (calibration, IQC, comparison or retained-sample retest) "
    "is selected according to impact on analytical performance (ODC-CX-12). Supplier "
    "changes require re-evaluation; new lots require verification before use; quality "
    "risk of near-expiry materials is assessed (ODC-CX-14). Examination-procedure "
    "changes are approved on an Examination Procedure Change Approval Form; the "
    "extent of verification/validation is determined by the impact (new reagent, "
    "instrument update, calibrator traceability change, or events severely affecting "
    "performance) (ODC-CX-20)."
)
T8_COMPANYWIDE = (
    "Yes. Unified change control is implemented company-wide. Change management is "
    "defined in multiple procedures covering documents (ODC-CX-33), examination "
    "procedures (ODC-CX-20), IT systems (ODC-CX-30), personnel (ODC-CX-09) and "
    "reagent suppliers (ODC-CX-14), including project and IT changes."
)
T8_PREAPPROVE = (
    "Yes. Changes are approved by the designated approver before execution, per "
    "ODC-CX-33 Document Control Procedure, ODC-CX-20 Selection, Verification and "
    "Validation of Examination Procedures, and ODC-CX-30 Computer Hardware and "
    "Software Management Procedure."
)
T8_IN_SYSTEM = (
    "Partially. Computerized-system changes are managed with defined electronic/"
    "controlled forms under ODC-CX-30 (software go-live/authorization: Computer "
    "Software Validation Record; new/changed tests: System New Test Application and "
    "Test Information Change Record; access rights: Information System Access Rights "
    "Record; database changes: Database Server Maintenance/Upgrade Record). Reagent "
    "procurement and inventory use OA and an intelligent reagent/consumable system "
    "(ODC-CX-14). Examination-procedure change requests and new-project reagent "
    "purchases are raised in OA (ODC-CX-20).\n"
    "Paper forms remain in use for UPS maintenance, breakdown repair, workstation "
    "rounds, HR files/training/competency, facility environmental records, equipment "
    "acceptance/maintenance/calibration, reagent QC/return, performance verification "
    "reports, and document issue/retrieval/destruction. Not all changes are managed "
    "in a single electronic system."
)
T8_EFFECTIVENESS = (
    "Yes. Effectiveness is reviewed after changes, including: retraining and "
    "reassessment of personnel before return to duty after role change, prolonged "
    "absence or procedure/technology change (ODC-CX-09); re-assessment of laboratory "
    "safety risk after facility change, with tracking of environmental excursions "
    "(ODC-CX-10); authorization to use equipment only after application/review/"
    "approval and performance verification following repair, storage or transfer "
    "(ODC-CX-12); supplier re-qualification and lot verification before use "
    "(ODC-CX-14); re-verification/validation after examination-procedure change "
    "(ODC-CX-20); software testing and Laboratory Director confirmation before "
    "go-live (ODC-CX-30); and document review/approval before release (ODC-CX-33)."
)
T8_CLIENTS = (
    "Accreditation-related changes are notified to CNAS within 20 working days "
    "(ODC-CX-09). Changes affecting examination procedures, equipment, reagents, "
    "IT systems or authorized personnel are controlled under the procedures above "
    "and are subject to approval and post-change verification. Customer/"
    "protocol-specific communication is executed as required by the relevant change, "
    "study protocol and quality agreements."
)

T9_SYSVAL = (
    "Yes. Systems used to generate laboratory data are controlled to ensure accuracy, "
    "reliability, consistent intended performance and detection of invalid or altered "
    "records. Equipment is qualified/verified before use and after repair "
    "(ODC-CX-12). Computerized systems are validated under ODC-CX-30. Paper records "
    "use controlled corrections (single-line strikethrough with signature/date "
    "annotation), anti-tampering controls and defined retention (clinical-trial "
    "records 25 years) to maintain traceability."
)
T9_EQ = (
    "Yes. Per ODC-CX-12 Equipment Management Procedure, newly purchased equipment "
    "shall complete acceptance and performance verification before use; after repair, "
    "equipment shall be verified as satisfactory before return to service."
)
T9_BACKUP = (
    "Yes. Per ODC-CX-32 Emergency Preparedness Procedure, in the event of instrument "
    "failure the supplier provides an equivalent-model replacement as an emergency "
    "back-up; the replacement shall complete performance verification before use."
)
T9_CSV_PROC = (
    "Yes. ODC-CX-30 Computer Hardware and Software Management Procedure defines "
    "software validation requirements and associated record forms."
)
T9_RBA = (
    "Per ODC-CX-30 Computer Hardware and Software Management Procedure, validation "
    "addresses functionality, performance, usability, compatibility and security. "
    "A risk-based approach is used: systems are risk-classified according to their "
    "impact on examination quality, patient safety and data integrity, and the scope "
    "and depth of validation are determined accordingly (aligned with GAMP 5 "
    "principles)."
)
T9_DI = (
    "Yes. Per ODC-CX-30, software validation shall ensure protection of sensitive "
    "data and prevent unauthorized access; system additions/changes shall be verified; "
    "database backups shall be verified for integrity. Risk-based computerized-system "
    "validation focuses on data acquisition, transmission, storage, modification, "
    "audit trail, backup/restore and access control to ensure data integrity."
)
T9_CFR11 = (
    "N/A, unless the relevant system is used for FDA-regulated electronic records/"
    "electronic signatures. Where applicable, assessment can be performed according "
    "to project requirements. A risk-based computerized-system validation approach "
    "is used, referring to GAMP 5 risk-management and validation-lifecycle concepts. "
    "Study-related instruments (ABI 7500 PCR and ABI 3500DX sequencer) provide audit-"
    "trail functionality; a LIS is not used for this project."
)

T10_RECEPTION = (
    "Yes. ODC-CX-19 Primary Sample Reception and Handling Procedure defines sample "
    "reconciliation, receipt, information entry, rejection of unsuitable samples and "
    "testing under concession."
)
T10_COLD = (
    "Yes. Per ODC-CX-18 Primary Sample Collection and Transport Procedure, samples "
    "are transported in rigid specimen boxes under temperature control, with transport "
    "temperature recorded. Receipt includes a transport-temperature check; if "
    "temperature is out of control, impact on results is assessed. Storage equipment "
    "is covered by a remote real-time temperature/humidity monitoring system "
    "(ODC-CX-32); abnormalities are handled immediately."
)
T10_INTEGRITY = (
    "Yes. Per ODC-CX-19 Primary Sample Reception and Handling Procedure, incoming "
    "samples are checked against acceptance criteria, including integrity, "
    "identification and collection time. Unsuitable samples are rejected or, after "
    "assessment, tested under concession with a documented remark."
)
T10_ID = (
    "Yes. Samples are identified by a unique 12-digit barcode, ensuring traceability "
    "from collection, receipt, transfer, distribution and examination through "
    "reporting (ODC-CX-19). Pathology reports use the pathology number as the unique "
    "identifier (ODC-CX-26 Examination Result Reporting Procedure)."
)
T10_STORAGE = (
    "Yes. Post-analysis retention follows ODC-SOP-CRO-01-012 Clinical Research Group "
    "Sample Reception and Handling Procedure. Retention of analysed samples complies "
    "with the clinical trial protocol, ethics approval and customer requirements, and "
    "samples are retained long-term until project completion."
)
T10_DISPOSAL = (
    "Yes. Per ODC-SOP-CRO-01-019 Clinical Research Group Post-Examination Sample "
    "Management Procedure, samples exceeding the retention period are disposed of "
    "safely as medical waste by the respective technical groups."
)
T10_TMON = (
    "Yes. Per ODC-CX-32, laboratory storage equipment is equipped with remote real-"
    "time temperature/humidity monitoring. Per ODC-CX-10, each technical group "
    "performs routine temperature/humidity monitoring and recording according to "
    "instrument and reagent requirements; out-of-control conditions are handled "
    "immediately."
)
T10_CONT = (
    "Yes. Per ODC-CX-32 Emergency Preparedness Procedure, storage equipment has "
    "remote real-time temperature/humidity monitoring. When equipment failure causes "
    "temperature to reach the alarm limit, the system simultaneously sends WeChat "
    "alerts to multiple persons, providing 24/7 monitoring and alarm notification."
)
T10_SPILL = (
    "Yes. Per ODC-CX-10, in the event of contaminant leakage or spillage, all "
    "contaminated surfaces (benches, floors, walls, etc.) and air are disinfected "
    "according to procedure. Chemical spills are handled per the emergency process "
    "in the Safety Manual (ODC-CX-32)."
)
T10_EQDB = (
    "Yes. Per ODC-CX-12 Equipment Management Procedure, the laboratory maintains an "
    "Equipment List and Equipment Basic Information Register covering identification, "
    "manufacturer information, calibration records and maintenance/repair records. "
    "Files are managed by the technical groups or the General Administration "
    "Department."
)
T10_EQVAL = (
    "Yes. Per ODC-CX-12, newly purchased equipment shall complete acceptance and "
    "performance verification; after repair, verification is required before reuse. "
    "Per ODC-CX-20, all examination procedures undergo independent verification "
    "before routine use."
)
T10_P11 = (
    "Yes. Study-related instruments including the ABI 7500 PCR system, ABI 3500DX "
    "sequencer and microplate reader are 21 CFR Part 11 compliant or have been "
    "evaluated against 21 CFR Part 11 requirements."
)
T10_SWITCH = (
    "Yes. Per ODC-CX-32, if an instrument fails mid-study, the supplier provides an "
    "equivalent-model replacement as emergency back-up; the replacement shall "
    "complete performance verification before use."
)
T10_MV_PROC = (
    "Yes. ODC-CX-20 Selection, Verification and Validation of Examination Procedures "
    "defines requirements for method verification and validation."
)
T10_MV_FLOW = (
    "Per ODC-CX-20, the verification/validation process is: prepare a protocol "
    "(including performance characteristics and acceptance criteria) → Technical "
    "Director approval → implementation by authorized personnel with raw-data "
    "retention → draft verification/validation report → technical group leader "
    "review → Technical Director approval."
)
T10_MV_EACH = (
    "Yes. Per ODC-CX-20, each new examination requires an Analytical Performance "
    "Verification Report or Analytical Performance Validation Report, reviewed and "
    "approved before implementation. Performance of approved examination procedures "
    "is reviewed at least annually."
)
T10_WHEN = (
    "Per ODC-CX-20 (ISO 15189 terminology):\n"
    "Verification is performed: before routine use; after events that severely affect "
    "analytical performance (e.g. major instrument component failure, relocation, "
    "severe environmental loss of control); and when elements of the measuring system "
    "change (reagent upgrade, instrument update, change in calibrator traceability, "
    "etc.).\n"
    "Validation is required for: non-standard methods; laboratory-developed methods; "
    "standard methods used outside their intended scope; and modified validated "
    "methods."
)
T10_PROVIDE_VR = (
    "Yes. Per ODC-CX-20, verification/validation reports are prepared by the test "
    "owner, reviewed by the technical group leader, approved by the Technical Director "
    "and archived by the document administrator. Reports can be provided upon request."
)
T10_TMF = "To be answered by VCB (Cerba Research)."
T10_SOP_COMP = (
    "Yes. SOPs for tests/parameters are compliant with ISO 15189 requirements "
    "(CNAS-CL02:2023 / ISO 15189:2022)."
)
T10_METHOD_INFO = (
    "Yes. Method, instrument, specimen requirements and volumes are specified in "
    "ODC-XMSC Project Manual and the corresponding work instructions (ODC-CX-18 "
    "Primary Sample Collection and Transport Procedure; ODC-CX-17 Examination Request "
    "Management Procedure)."
)
T10_REVAL = (
    "Yes. Re-verification/adjustment is required when: the examination procedure "
    "changes (ODC-CX-20); the reagent lot changes (ODC-CX-14); or the QC lot changes, "
    "in which case the mean and SD are re-accumulated (ODC-CX-23 Internal Quality "
    "Control Procedure)."
)
T10_REVAL_FREQ = (
    "Examination-procedure change: as needed (event-driven).\n"
    "Reagent lot change: at each lot change.\n"
    "QC lot change: at each lot change (re-evaluation after accumulating 20 days of "
    "data)."
)
T10_EQA = (
    "Yes. Per ODC-CX-24 Proficiency Testing and Interlaboratory Comparison Procedure, "
    "the laboratory preferentially participates in PT/EQA schemes organized by the "
    "NCCL (National Center for Clinical Laboratories) and the Shanghai Center for "
    "Clinical Laboratory."
)
T10_EQA_WHEN = (
    "Per ODC-CX-24: EQA/PT is applicable for examinations that have an available "
    "EQA/PT scheme and for newly introduced examinations (new tests preferentially "
    "use measurement audit or interlaboratory comparison). Where no PT scheme exists "
    "and interlaboratory comparison is not suitable, alternative approaches may be "
    "used after authorization and shall be documented."
)
T10_IQC = (
    "Yes. Per ODC-CX-23 Internal Quality Control Procedure, in principle IQC is "
    "performed for all examinations offered; at least one QC determination is run on "
    "each testing day or each analytical run."
)
T10_IQC_WHEN = (
    "Per ODC-CX-23, IQC is used for examinations in routine service: at least one QC "
    "determination per testing day or analytical run. IQC is not omitted for offered "
    "tests except where the documented procedure defines an authorized alternative."
)
T10_LIMS = (
    "No. Analytical instruments are not directly interfaced with a LIMS."
)
T10_DATAFMT = (
    "Data are transferred via the Datalink website in PDF and JPG formats."
)
T10_COMP = (
    "Yes. Per ODC-CX-09 Human Resource Management Procedure, competency of laboratory "
    "personnel for analytical procedures is assessed periodically:\n"
    "Existing staff: at least once per year.\n"
    "New staff: at least two competency assessments within the first 6 months.\n"
    "Assessment covers technical skills and theoretical knowledge; staff who do not "
    "pass shall be retrained and reassessed before working independently.\n"
    "Before this project starts, the customer provides study-specific training and "
    "competency assessment covering the project method, sample receipt/handling, raw-"
    "data recording, compliance requirements and deviation handling. Only personnel "
    "authorized after theoretical and practical assessment may perform project "
    "testing. If methods or project requirements change, supplementary training and "
    "reassessment are arranged."
)
T10_EXPERTISE = (
    "Yes. The laboratory has end-to-end technical capability for selection, "
    "verification and validation of examination procedures, internal QC, proficiency "
    "testing and internal comparison (ODC-CX-20, ODC-CX-23, ODC-CX-24, ODC-CX-25). "
    "Technical groups include molecular, pathology, microbiology and clinical "
    "research (ODC-CX-33 Document Control Procedure)."
)
T10_SUB = "No. Subcontractors are not used for tests provided to Cerba Research. N/A."


def fill(doc: Document) -> None:
    t1, t2, t3, t4 = doc.tables[1], doc.tables[2], doc.tables[3], doc.tables[4]
    t8, t9, t10 = doc.tables[8], doc.tables[9], doc.tables[10]
    t5, t6, t7 = doc.tables[5], doc.tables[6], doc.tables[7]

    # ----- Table 1 company profile (do not overwrite existing filled fields) -----
    fill_comment(t1, 7, T1_FUNCTION)
    fill_comment(t1, 8, T1_PHONE)
    fill_comment(t1, 9, T1_EMAIL)
    fill_comment(t1, 10, T1_BUSINESS)
    fill_comment(t1, 11, T1_HOW_LONG)

    loc_cell = unique_cells(t1.rows[12])[-1]
    check_sdt_paragraphs(loc_cell, [2])  # Asia-Pacific

    exp_cell = unique_cells(t1.rows[13])[-1]
    check_sdt_paragraphs(exp_cell, [0, 1, 3, 4])  # special, safety, processing, MV
    # append other expertise to last paragraph if still a prompt
    last_p = exp_cell.paragraphs[5]
    if "Other, please specify:" in last_p.text and "IHC" not in last_p.text:
        set_paragraph_text(last_p, T1_OTHER_EXPERTISE)

    fill_comment(t1, 14, T1_SUPPLEMENTAL)
    fill_comment(t1, 15, T1_TURNOVER)

    mark_inline_yes(unique_cells(t1.rows[18])[-1])
    fill_comment(t1, 20, T1_PARENT)
    cust_cells = unique_cells(t1.rows[23])
    for c in cust_cells:
        if is_placeholder(cell_text(c)):
            set_cell_text(c, T1_CUSTOMER)

    mark_inline_yes(unique_cells(t1.rows[26])[-1])  # regulatory inspection Yes
    mark_inline_yes(unique_cells(t1.rows[30])[-1])  # debarment confirmation Yes
    # certification Yes + ISO15189 note if still placeholder-like
    cert_cell = unique_cells(t1.rows[31])[-1]
    mark_inline_yes(cert_cell)
    # ISO 15189 details on first empty data row after certificate request
    # R033 = "Please provide the Certificates" header; R034 empty data
    std_row = unique_cells(t1.rows[34])
    if len(std_row) >= 4 and not cell_text(std_row[0]).strip():
        set_cell_text(std_row[0], "ISO 15189", force=True)
        set_cell_text(std_row[1], "China National Accreditation Service for Conformity Assessment (CNAS)", force=True)
        set_cell_text(std_row[2], "12 March 2026", force=True)
        set_cell_text(std_row[3], "10 January 2030", force=True)

    # ----- Table 2 QMS -----
    mark_yna(t2.rows[2], "yes")
    fill_comment(t2, 2, T2_QMS_YES)
    fill_comment(t2, 3, T2_FRAMEWORK)
    mark_yna(t2.rows[4], "yes")
    fill_comment(t2, 4, T2_QM)
    mark_yna(t2.rows[5], "yes")
    fill_comment(t2, 5, T2_QA_DEPT)
    fill_comment(t2, 6, T2_QA_RESP)
    mark_yna(t2.rows[7], "yes")
    fill_comment(t2, 7, T2_COVERED)
    add_check_to_para(unique_cells(t2.rows[8])[-1], 2)  # combination of both
    fill_comment(t2, 9, T2_SOP_ACCESS)
    fill_comment(t2, 10, T2_OBSOLETE)
    mark_yna(t2.rows[11], "yes")
    fill_comment(t2, 11, T2_REVISION)
    fill_comment(t2, 12, T2_GDP)

    # ----- Table 3 facilities -----
    mark_yna(t3.rows[2], "yes")
    fill_comment(t3, 2, T3_SECURITY)
    fill_comment(t3, 3, T3_OVERVIEW)
    mark_yna(t3.rows[4], "yes")
    fill_comment(t3, 4, T3_PEST)
    mark_yna(t3.rows[5], "yes")
    fill_comment(t3, 5, T3_PEST_INSP)
    mark_yna(t3.rows[6], "yes")
    fill_comment(t3, 6, T3_TEMP)
    fill_comment(t3, 7, T3_OOS)
    mark_yna(t3.rows[8], "no")  # comment states no generator
    fill_comment(t3, 8, T3_GENERATOR)
    mark_yna(t3.rows[9], "na")
    fill_comment(t3, 9, T3_GENERATOR_TEST)
    mark_yna(t3.rows[10], "yes")
    fill_comment(t3, 10, T3_BCP)
    fill_comment(t3, 11, T3_BCP_TEST)
    mark_yna(t3.rows[12], "yes")
    fill_comment(t3, 12, T3_BCP_RISK)

    # ----- Table 4 training: fill empty comments + Yes/No; keep existing English -----
    mark_yna(t4.rows[2], "yes")
    fill_comment(t4, 2, T4_JD)
    train_cell = unique_cells(t4.rows[3])[-1]
    for i in range(6):
        add_check_to_para(train_cell, i)
    # GDPR extra note
    gdpr_p = train_cell.paragraphs[2]
    if "ODC-CX-04" not in gdpr_p.text:
        gdpr_p.add_run(T4_GDPR_NOTE)
    mark_yna(t4.rows[4], "yes")
    fill_comment(t4, 4, T4_GENERAL)
    fill_comment(t4, 5, T4_OVERALL)
    mark_yna(t4.rows[6], "yes")
    mark_yna(t4.rows[8], "yes")
    mark_yna(t4.rows[11], "yes")
    mark_yna(t4.rows[13], "na")
    mark_yna(t4.rows[14], "yes")

    # ----- Tables 5–7: Yes/No only (comments already filled) -----
    for ri, choice in [
        (2, "yes"),
        (3, "yes"),
        (4, "yes"),
        (5, "yes"),
        (7, "yes"),
        (8, "yes"),
        (9, "no"),
    ]:
        mark_yna(t5.rows[ri], choice)

    for ri in (2, 4, 6, 7, 8, 9):
        mark_yna(t6.rows[ri], "yes")

    add_check_to_para(unique_cells(t7.rows[2])[-1], 2)  # electronic and paper-based
    mark_yna(t7.rows[3], "yes")  # skipped if cell already has "d"
    mark_yna(t7.rows[5], "yes")
    mark_yna(t7.rows[6], "yes")
    mark_yna(t7.rows[7], "yes")
    mark_yna(t7.rows[8], "yes")
    mark_yna(t7.rows[9], "no")
    mark_yna(t7.rows[10], "na")
    mark_yna(t7.rows[11], "yes")
    mark_yna(t7.rows[13], "na")
    mark_yna(t7.rows[14], "na")

    # ----- Table 8 change management -----
    mark_yna(t8.rows[2], "yes")
    fill_comment(t8, 2, T8_CM)
    mark_yna(t8.rows[3], "yes")
    fill_comment(t8, 3, T8_RISK)
    fill_comment(t8, 4, T8_COMPANYWIDE)
    mark_yna(t8.rows[5], "yes")
    fill_comment(t8, 5, T8_PREAPPROVE)
    mark_yna(t8.rows[6], "yes")
    fill_comment(t8, 6, T8_IN_SYSTEM)
    mark_yna(t8.rows[7], "yes")
    fill_comment(t8, 7, T8_EFFECTIVENESS)
    mark_yna(t8.rows[8], "yes")
    fill_comment(t8, 8, T8_CLIENTS)

    # ----- Table 9 system validation -----
    mark_yna(t9.rows[2], "yes")
    fill_comment(t9, 2, T9_SYSVAL)
    mark_yna(t9.rows[3], "yes")
    fill_comment(t9, 3, T9_EQ)
    mark_yna(t9.rows[4], "yes")
    fill_comment(t9, 4, T9_BACKUP)
    mark_yna(t9.rows[5], "yes")
    fill_comment(t9, 5, T9_CSV_PROC)
    fill_comment(t9, 6, T9_RBA)
    mark_yna(t9.rows[7], "yes")
    fill_comment(t9, 7, T9_DI)
    mark_yna(t9.rows[8], "na")
    fill_comment(t9, 8, T9_CFR11)

    # ----- Table 10 laboratory-specific -----
    items = [
        (3, "yes", T10_RECEPTION),
        (4, "yes", T10_COLD),
        (5, "yes", T10_INTEGRITY),
        (6, "yes", T10_ID),
        (7, "yes", T10_STORAGE),
        (8, "yes", T10_DISPOSAL),
        (9, "yes", T10_TMON),
        (11, "yes", T10_CONT),
        (12, "yes", T10_SPILL),
        (14, "yes", T10_EQDB),
        (16, "yes", T10_EQVAL),
        (17, "yes", T10_P11),
        (18, "yes", T10_SWITCH),
        (20, "yes", T10_MV_PROC),
        (22, "yes", T10_MV_EACH),
        (24, "yes", T10_PROVIDE_VR),
        (27, "yes", T10_SOP_COMP),
        (28, "yes", T10_METHOD_INFO),
        (29, "yes", T10_REVAL),
        (31, "yes", T10_EQA),
        (33, "yes", T10_IQC),
        (35, "no", T10_LIMS),
        (37, "yes", T10_COMP),  # Chinese note: should be Yes
        (38, "yes", T10_EXPERTISE),
        (39, "no", T10_SUB),
    ]
    for ri, choice, text in items:
        mark_yna(t10.rows[ri], choice)
        fill_comment(t10, ri, text)

    fill_comment(t10, 21, T10_MV_FLOW)
    fill_comment(t10, 23, T10_WHEN)
    fill_comment(t10, 25, T10_TMF)
    fill_comment(t10, 30, T10_REVAL_FREQ)
    fill_comment(t10, 32, T10_EQA_WHEN)
    fill_comment(t10, 34, T10_IQC_WHEN)
    fill_comment(t10, 36, T10_DATAFMT)

    temp_cell = unique_cells(t10.rows[10])[-1]
    check_sdt_paragraphs(temp_cell, [0, 1, 2, 3])  # RT, 2-8, -20, -80; not LN2

    # subcontractors N/A rows
    for ri in (41, 42, 43, 44):
        cells = unique_cells(t10.rows[ri])
        if len(cells) >= 2 and is_placeholder(cell_text(cells[1])):
            set_cell_text(cells[1], "N/A")
        if len(cells) >= 3 and is_placeholder(cell_text(cells[-1])):
            set_cell_text(cells[-1], "N/A")
        elif len(cells) == 2 and is_placeholder(cell_text(cells[-1])):
            set_cell_text(cells[-1], "N/A")


def main() -> None:
    doc = Document(str(SRC))
    fill(doc)
    doc.save(str(DST))
    print("saved", DST)


if __name__ == "__main__":
    main()
