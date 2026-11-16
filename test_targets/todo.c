/*
 * todo.c - a small single-file todo manager, used as a "realistic"
 * test target for cbounded. strcpy() calls are spread across several
 * layers of helpers, so most verdicts depend on the call graph.
 *
 * Tags on each strcpy (and on interesting call sites):
 *   SAFE    - always fits
 *   UNSAFE  - can overflow on some path
 *   UNKNOWN - not decidable inside this function; see its callers
 *
 * Call graph (roughly):
 *
 *   main
 *    +- todo_list_init -> set_db_path
 *    +- load_from_file -> parse_record -> todo_set_title -> str_copy_bounded
 *    |                                 -> todo_set_owner
 *    |                                 -> todo_set_note
 *    |                                 -> todo_set_status -> status_name
 *    +- repl -> parse_command
 *            -> dispatch -> cmd_add    -> todo_new -> todo_set_title
 *                                                  -> todo_set_owner(default_owner())
 *                        -> cmd_done   -> todo_set_status
 *                        -> cmd_start  -> todo_set_status
 *                        -> cmd_rename -> remember_title, todo_set_title
 *                        -> cmd_undo
 *                        -> cmd_assign -> todo_set_owner
 *                        -> cmd_note   -> todo_set_note
 *                        -> cmd_find   -> str_copy_bounded
 *                        -> cmd_list   -> print_todo    -> format_todo -> append
 *                                      -> print_compact -> format_todo -> append
 *                        -> cmd_save   -> save_to_file  -> format_record -> append
 *
 * Usage: todo [db-file]   then type commands, "help" for a list.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE_MAX 64
#define OWNER_MAX 16
#define NOTE_MAX 128
#define STATUS_MAX 8
#define DB_PATH_MAX 128
#define UNDO_MAX 32
#define BUF_LINE 512
#define MAX_TODOS 64

enum status { ST_TODO, ST_DONE, ST_IN_PROGRESS };

struct todo {
	int id;
	char title[TITLE_MAX];
	char owner[OWNER_MAX];
	char note[NOTE_MAX];
	char status[STATUS_MAX];
};

struct todo_list {
	struct todo items[MAX_TODOS];
	size_t count;
	int next_id;
	char db_path[DB_PATH_MAX];
	int last_id;
	char last_title[UNDO_MAX];
};

/* ------------------------------------------------------------ helpers */

/* Correct as long as dstsz really is the size of dst. */
static int str_copy_bounded(char *dst, size_t dstsz, const char *src)
{
	if (strlen(src) >= dstsz)
		return -1;
	strcpy(dst, src); /* SAFE given an honest dstsz */
	return 0;
}

/* strcpy and return a pointer to the new end of the string. */
static char *append(char *dst, const char *src)
{
	strcpy(dst, src); /* UNKNOWN: depends on caller's remaining space */
	return dst + strlen(src);
}

static char *trim(char *s)
{
	char *end;

	while (isspace((unsigned char)*s))
		s++;
	end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

static const char *status_name(enum status st)
{
	switch (st) {
	case ST_DONE:
		return "done";
	case ST_IN_PROGRESS:
		return "in-progress"; /* 12 bytes, status[] holds 8 */
	case ST_TODO:
	default:
		return "todo";
	}
}

static enum status status_parse(const char *s)
{
	if (strcmp(s, "done") == 0)
		return ST_DONE;
	if (strcmp(s, "in-progress") == 0)
		return ST_IN_PROGRESS;
	return ST_TODO;
}

static const char *default_owner(void)
{
	const char *user = getenv("USER");
	return user ? user : "nobody";
}

/* ------------------------------------------------------- todo setters */

static int todo_set_title(struct todo *t, const char *title)
{
	return str_copy_bounded(t->title, sizeof(t->title), title); /* SAFE */
}

/* No check: safety depends entirely on callers. */
static void todo_set_owner(struct todo *t, const char *owner)
{
	strcpy(t->owner, owner); /* UNKNOWN: see callers */
}

static void todo_set_note(struct todo *t, const char *note)
{
	if (strlen(note) < sizeof(t->note))
		strcpy(t->note, note); /* SAFE */
	else
		t->note[0] = '\0';
}

static void todo_set_status(struct todo *t, enum status st)
{
	strcpy(t->status, status_name(st)); /* UNSAFE when ST_IN_PROGRESS */
}

/* ---------------------------------------------------------- todo list */

static void set_db_path(struct todo_list *l, const char *override)
{
	const char *home;
	char *p;

	if (override) {
		strcpy(l->db_path, override); /* UNSAFE: argv[1], unbounded */
		return;
	}
	home = getenv("HOME");
	if (!home) {
		strcpy(l->db_path, ".todo"); /* SAFE */
		return;
	}
	p = append(l->db_path, home); /* UNSAFE: $HOME is unbounded */
	append(p, "/.todo"); /* UNSAFE: follows the above */
}

static void todo_list_init(struct todo_list *l, const char *path)
{
	memset(l, 0, sizeof(*l));
	l->next_id = 1;
	l->last_id = -1;
	set_db_path(l, path);
}

static struct todo *todo_find(struct todo_list *l, int id)
{
	for (size_t i = 0; i < l->count; i++)
		if (l->items[i].id == id)
			return &l->items[i];
	return NULL;
}

static struct todo *todo_new(struct todo_list *l, const char *title)
{
	struct todo *t;

	if (l->count >= MAX_TODOS)
		return NULL;
	t = &l->items[l->count];
	memset(t, 0, sizeof(*t));
	if (todo_set_title(t, title) != 0)
		return NULL;
	t->id = l->next_id++;
	todo_set_owner(t, default_owner()); /* UNSAFE: $USER is unbounded */
	todo_set_status(t, ST_TODO); /* SAFE: "todo" */
	l->count++;
	return t;
}

/* ---------------------------------------------------------- rendering */

/*
 * Worst case: "#" + 11-digit id + " [" + 7 status + "] " + 63 title +
 * " @" + 15 owner + NUL = 104 bytes. Callers must provide that much.
 */
static void format_todo(const struct todo *t, char *out)
{
	char idbuf[16];
	char *p = out;

	snprintf(idbuf, sizeof(idbuf), "%d", t->id);
	p = append(p, "#");
	p = append(p, idbuf);
	p = append(p, " [");
	p = append(p, t->status);
	p = append(p, "] ");
	p = append(p, t->title);
	p = append(p, " @");
	append(p, t->owner);
}

static void print_todo(const struct todo *t)
{
	char line[128];

	format_todo(t, line); /* SAFE: 128 >= 104 */
	puts(line);
	if (t->note[0])
		printf("    note: %s\n", t->note);
}

static void print_compact(const struct todo *t)
{
	char line[48];

	format_todo(t, line); /* UNSAFE: 48 < 104 */
	puts(line);
}

/*
 * id|status|owner|title|note
 * Worst case: 11 + 1 + 7 + 1 + 15 + 1 + 63 + 1 + 127 + NUL = 228.
 */
static void format_record(const struct todo *t, char *out)
{
	char idbuf[16];
	char *p = out;

	snprintf(idbuf, sizeof(idbuf), "%d", t->id);
	p = append(p, idbuf);
	p = append(p, "|");
	p = append(p, t->status);
	p = append(p, "|");
	p = append(p, t->owner);
	p = append(p, "|");
	p = append(p, t->title);
	p = append(p, "|");
	append(p, t->note);
}

/* --------------------------------------------------------- persistence */

static int parse_record(struct todo_list *l, char *line)
{
	char *id_s, *status_s, *owner_s, *title_s, *note_s;
	struct todo *t;

	id_s = strtok(line, "|");
	status_s = strtok(NULL, "|");
	owner_s = strtok(NULL, "|");
	title_s = strtok(NULL, "|");
	note_s = strtok(NULL, "\n");
	if (!id_s || !status_s || !owner_s || !title_s)
		return -1;
	if (l->count >= MAX_TODOS)
		return -1;

	t = &l->items[l->count];
	memset(t, 0, sizeof(*t));
	t->id = atoi(id_s);
	if (todo_set_title(t, title_s) != 0) /* SAFE */
		return -1;
	todo_set_owner(t, owner_s); /* UNSAFE: field from file, unbounded */
	todo_set_status(t, status_parse(status_s)); /* UNSAFE: may be in-progress */
	if (note_s)
		todo_set_note(t, note_s); /* SAFE */

	if (t->id >= l->next_id)
		l->next_id = t->id + 1;
	l->count++;
	return 0;
}

static void load_from_file(struct todo_list *l)
{
	char line[BUF_LINE];
	FILE *f = fopen(l->db_path, "r");

	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
		parse_record(l, line);
	fclose(f);
}

static int save_to_file(const struct todo_list *l)
{
	char rec[256];
	FILE *f = fopen(l->db_path, "w");

	if (!f)
		return -1;
	for (size_t i = 0; i < l->count; i++) {
		format_record(&l->items[i], rec); /* SAFE: 256 >= 228 */
		fprintf(f, "%s\n", rec);
	}
	fclose(f);
	return 0;
}

/* ------------------------------------------------------------ commands */

static void remember_title(struct todo_list *l, const struct todo *t)
{
	l->last_id = t->id;
	strcpy(l->last_title, t->title); /* UNSAFE: 64-byte title into 32 */
}

static void cmd_add(struct todo_list *l, const char *arg)
{
	struct todo *t = todo_new(l, arg);

	if (!t) {
		puts("could not add (full or title too long)");
		return;
	}
	printf("added #%d\n", t->id);
}

static void cmd_done(struct todo_list *l, const char *arg)
{
	struct todo *t = todo_find(l, atoi(arg));

	if (t)
		todo_set_status(t, ST_DONE); /* SAFE: "done" */
}

static void cmd_start(struct todo_list *l, const char *arg)
{
	struct todo *t = todo_find(l, atoi(arg));

	if (t)
		todo_set_status(t, ST_IN_PROGRESS); /* UNSAFE: "in-progress" */
}

static void cmd_rename(struct todo_list *l, const char *arg)
{
	char *end;
	long id = strtol(arg, &end, 10);
	struct todo *t = todo_find(l, (int)id);

	if (!t || end == arg)
		return;
	remember_title(l, t);
	if (todo_set_title(t, trim(end)) != 0) /* SAFE */
		puts("title too long");
}

static void cmd_undo(struct todo_list *l, const char *arg)
{
	struct todo *t = todo_find(l, l->last_id);

	(void)arg;
	if (!t)
		return;
	strcpy(t->title, l->last_title); /* SAFE: 32 into 64 */
	l->last_id = -1;
}

static void cmd_assign(struct todo_list *l, const char *arg)
{
	int id;
	char who[OWNER_MAX];
	struct todo *t;

	if (sscanf(arg, "%d %15s", &id, who) != 2)
		return;
	t = todo_find(l, id);
	if (t)
		todo_set_owner(t, who); /* SAFE: %15s bounds it */
}

static void cmd_note(struct todo_list *l, const char *arg)
{
	char *end;
	long id = strtol(arg, &end, 10);
	struct todo *t = todo_find(l, (int)id);

	if (t && end != arg)
		todo_set_note(t, trim(end)); /* SAFE: checked inside */
}

static void cmd_find(struct todo_list *l, const char *arg)
{
	char needle[32];

	/* Bug: passes TITLE_MAX instead of sizeof(needle). */
	if (str_copy_bounded(needle, TITLE_MAX, arg) != 0) /* UNSAFE */
		return;
	for (size_t i = 0; i < l->count; i++)
		if (strstr(l->items[i].title, needle))
			print_todo(&l->items[i]);
}

static void cmd_list(struct todo_list *l, const char *arg)
{
	int compact = arg[0] == 'c';

	for (size_t i = 0; i < l->count; i++) {
		if (compact)
			print_compact(&l->items[i]);
		else
			print_todo(&l->items[i]);
	}
}

static void cmd_save(struct todo_list *l, const char *arg)
{
	(void)arg;
	if (save_to_file(l) != 0)
		perror(l->db_path);
}

static void cmd_help(struct todo_list *l, const char *arg)
{
	(void)l;
	(void)arg;
	puts("add <title> | done <id> | start <id> | rename <id> <title>\n"
	     "undo | assign <id> <who> | note <id> <text> | find <text>\n"
	     "list [c] | save | quit");
}

/* ---------------------------------------------------------------- REPL */

struct command {
	const char *name;
	void (*fn)(struct todo_list *, const char *);
};

static const struct command commands[] = {
	{ "add", cmd_add },	  { "done", cmd_done },
	{ "start", cmd_start },	  { "rename", cmd_rename },
	{ "undo", cmd_undo },	  { "assign", cmd_assign },
	{ "note", cmd_note },	  { "find", cmd_find },
	{ "list", cmd_list },	  { "save", cmd_save },
	{ "help", cmd_help },
};

/* Splits "cmd rest..." into cmd (bounded) and rest. */
static int parse_command(const char *line, char cmd[16], char rest[BUF_LINE])
{
	const char *p;

	if (sscanf(line, "%15s", cmd) != 1)
		return -1;
	p = strstr(line, cmd) + strlen(cmd);
	while (isspace((unsigned char)*p))
		p++;
	strcpy(rest, p); /* SAFE: p is a suffix of a BUF_LINE line */
	return 0;
}

static int dispatch(struct todo_list *l, const char *cmd, const char *rest)
{
	if (strcmp(cmd, "quit") == 0)
		return 1;
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
		if (strcmp(cmd, commands[i].name) == 0) {
			commands[i].fn(l, rest);
			return 0;
		}
	}
	printf("unknown command: %s\n", cmd);
	return 0;
}

static void repl(struct todo_list *l)
{
	char line[BUF_LINE];
	char cmd[16];
	char rest[BUF_LINE];

	for (;;) {
		fputs("> ", stdout);
		fflush(stdout);
		if (!fgets(line, sizeof(line), stdin))
			break;
		if (parse_command(line, cmd, rest) != 0)
			continue;
		trim(rest);
		if (dispatch(l, cmd, rest))
			break;
	}
}

int main(int argc, char **argv)
{
	static struct todo_list list;

	todo_list_init(&list, argc > 1 ? argv[1] : NULL);
	load_from_file(&list);
	repl(&list);
	return 0;
}
