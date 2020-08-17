#include <stdio.h>
#include <malloc.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/* initialize a list head explicitly */
static inline void INIT_LIST_HEAD(struct list_head *p)
{
	p->next = p->prev = p;
}


static inline void list_add_tail(struct list_head *p, struct list_head *list)
{
	struct list_head *last = list->prev;

	last->next = p;
	p->prev = last;
	p->next = list;
	list->prev = p;
}


#define list_entry_offset(p, type, offset) \
	((type *)((char *)(p) - (offset)))

#define list_entry(p, type, member) \
	list_entry_offset(p, type, offsetof(type, member))

/* list_for_each - iterate over the linked list
 * @p: iterator, a list_head pointer variable
 * @list: list_head pointer containing the list
 */
#define list_for_each(p, list) \
	for (p = (list)->next; p != (list); p = p->next)


typedef struct snd_config snd_config_t;

struct snd_config {
	char *id;
	int type;
	union {
		char *string;
		long integer;
		struct {
			struct list_head fields;
			bool join;
		}compound;
	}u;
	struct list_head list;
	snd_config_t *parent;
};


typedef enum _snd_config_type {
	/** Integer number. */
        SND_CONFIG_TYPE_INTEGER,
	/** 64-bit integer number. */
        SND_CONFIG_TYPE_INTEGER64,
	/** Real number. */
        SND_CONFIG_TYPE_REAL,
	/** Character string. */
        SND_CONFIG_TYPE_STRING,
        /** Pointer (runtime only, cannot be saved). */
        SND_CONFIG_TYPE_POINTER,
	/** Compound node. */
	SND_CONFIG_TYPE_COMPOUND = 1024
} snd_config_type_t;

char *test_array[6] = {"SectionData", "SOF_ABI", "bytes", "0x10,0x11,0x12"}; 

#define MAX_LEVEL 4


static int _snd_config_make(snd_config_t **config, char **id, snd_config_type_t type)
{
	snd_config_t *n;
	assert(config);
	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		if (*id) {
			free(*id);
			*id = NULL;
		}
		return -ENOMEM;
	}
	if (id) {
		n->id = *id;
		*id = NULL;
	}
	n->type = type;
	if (type == SND_CONFIG_TYPE_COMPOUND)
		INIT_LIST_HEAD(&n->u.compound.fields);
	*config = n;

	printf("%s: id=%s, n=%p\n", __FUNCTION__, n->id, n);
	return 0;
}

static int _snd_config_make_add(snd_config_t **config, char **id,
				snd_config_type_t type, snd_config_t *parent)
{
	snd_config_t *n;
	int err;
	assert(parent->type == SND_CONFIG_TYPE_COMPOUND);
	err = _snd_config_make(&n, id, type);
	if (err < 0)
		return err;
	n->parent = parent;
	list_add_tail(&n->list, &parent->u.compound.fields);
	*config = n;
	return 0;
}

void main(void)
{

	struct snd_config *config;
	struct snd_config *parent;
	struct snd_config *first;

	struct list_head *list;
	struct list_head *loop;
	int i = 0;
	char *id = test_array[i];
	snd_config_type_t type;

	_snd_config_make(&config, &id, SND_CONFIG_TYPE_COMPOUND);
	parent = config;
	first = config;
	
	printf("first=%p, &first=%p\n", first, &first);

	printf("config=%p, &config=%p\n", config, &config);

	for (i = 1; i < MAX_LEVEL; i++) {
		if (i < MAX_LEVEL)
			type = SND_CONFIG_TYPE_COMPOUND;
		else
			type = SND_CONFIG_TYPE_STRING;
		id = test_array[i];
		printf("%d:id=%s\n", i, id);
		_snd_config_make_add(&config, &id, type, parent);
		parent = config;

		printf("%d:config=%p, &config=%p\n", i, config, &config);
	}
	
	
	/*print list info*/
	list = &first->u.compound.fields;
	config = list_entry(list, snd_config_t, u);
	printf("id=%s, type=%d\n", config->id, config->type);
	
	while (config != NULL) {
		config = NULL;
		list_for_each(loop, list) {
			config = list_entry(loop, snd_config_t, list);
			printf("id=%s, type=%d, config=%p\n", config->id, config->type, config);
		}
		if (config != NULL)
			list = &config->u.compound.fields;
		else
			break;

	}
	
	printf("--------------Done-----------------\n");

}
