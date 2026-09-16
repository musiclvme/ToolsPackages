# PDF 页面解析 / 导出器

从一份 PDF 里查看页数、书签和文本，并按指定页码无损导出成新的 PDF。不绑定某一份文件，任意 PDF 都可以用。

## 安装

```bash
cd pdf_parser
python -m pip install -r requirements.txt
```

仓库根目录也可以用 `python -m pdf_parser ...`。Windows 建议先进入 `pdf_parser` 目录。

## 命令行

```bash
# 查看页数、元数据和书签
python cli.py info input.pdf

# 列出书签（方便按章节挑页）
python cli.py bookmarks input.pdf

# 导出第 1-5、8、10 页到新文件
python cli.py export input.pdf -p 1-5,8,10 -o selected.pdf

# 只要奇数页 / 偶数页
python cli.py export input.pdf -p odd -o odd-pages.pdf
python cli.py export input.pdf -p even -o even-pages.pdf

# 从第 10 页到末页
python cli.py export input.pdf -p 10- -o from-page-10.pdf

# 抽取指定页文本
python cli.py text input.pdf -p 1-3
```

页码从 **1** 开始。支持：

- 单页：`1,3,8`
- 闭区间：`5-9`
- 开区间：`10-`（到末页）、`-4`（从第 1 页到第 4 页）
- `odd` / `even`
- 加密 PDF：加 `--password`

## 网页界面

```bash
python cli.py serve
```

浏览器打开 http://127.0.0.1:5055

1. 拖入或选择 PDF
2. 在缩略图上勾选页面，或在左侧输入 `1-5,8,10`
3. 可点击书签把对应页加入选择
4. 点「导出选中页面」，下载新的 PDF

## 运行测试

```bash
python -m pip install pytest
python -m pytest tests/test_pdf_parser.py
```
