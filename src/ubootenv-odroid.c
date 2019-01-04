static pthread_mutex_t env_lock = PTHREAD_MUTEX_INITIALIZER;

char PROFIX_UBOOTENV_VAR[32] = { 0, };

static size_t ENV_PARTITIONS_SIZE = 0;
static int ENV_INIT_DONE = 0;

static struct environment env_data;
static struct env_attribute env_attribute_header;

extern char **environ;

struct {
	const char *key;
	const char *rep;
} env_quirk[] = {
	{ "vout", "outputmode" },
};

const char *env_key_quirk(const char *key)
{
	int i;

	for (i = 0; i < sizeof(env_quirk) / sizeof(env_quirk[0]); i++) {
		if (!strcmp(key, env_quirk[i].key))
			return env_quirk[i].rep;
	}
	return key;
}

static env_attribute *env_parse_attribute(void)
{
	char *data = env_data.data;
	env_attribute_t *attr = &env_attribute_header;

	syslog(LOG_INFO, "[ubootenv] env data=[%s]\n", __func__, __LINE__, data);

	memset(attr, 0, sizeof(env_attribute_t));

	/* Split string to tokens */
	char *token = strtok(data, " ");
	while (token) {
		char *p = strstr(token, "=");
		if (p) {
			*(char *)p = '\0';
			const char *key = env_key_quirk(token);
			char *val = p + 1;

			/* FIXME: take the first value in the values */
			p = strstr(val, ",");
			if (p)
				*(char*)p = '\0';

			strcpy(attr->key, key);
			strcpy(attr->value, val);

			attr->next = (env_attribute_t *)malloc(sizeof(env_attribute_t));
			if (!attr->next)
				break;
			memset(attr->next, 0, sizeof(env_attribute_t));
			attr = attr->next;
		}

		token = strtok(NULL, " ");
	}

	return &env_attribute_header;
}

static int env_revert_attribute(void)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

env_attribute_t *bootenv_get_attr(void)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return NULL;
}

void bootenv_print(void)
{
	env_attribute *attr;

	for (attr = &env_attribute_header; attr; attr = attr->next)
		syslog(LOG_INFO, "[ubootenv] key(%s) value(%s)", attr->key, attr->value);
}

int read_bootenv()
{
	env_attribute_t *attr;

	char *buf = (char*)malloc(ENV_PARTITIONS_SIZE);
	if (!buf)
		return -ENOMEM;

	memset(buf, 0, ENV_PARTITIONS_SIZE);

	env_data.data = buf;

	int fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0) {
		syslog(LOG_ERR, "[ubootenv] failed to access (%s)\n", strerror(errno));
		return -errno;
	}
	int siz = read(fd, buf, ENV_PARTITIONS_SIZE);
	if (siz <= 0) {
		syslog(LOG_ERR, "[ubootenv] failed to read (%s)\n", strerror(errno));
		close(fd);
		return -errno;
	}
	close(fd);

	*(char *)(buf + siz - 1) = '\0';

	pthread_mutex_lock(&env_lock);
	attr = env_parse_attribute();
	pthread_mutex_unlock(&env_lock);

	if (!attr)
		return -EINVAL;

	bootenv_print();

	return 0;
}

const char *bootenv_get_value(const char *key)
{
	env_attribute_t *attr = &env_attribute_header;

	if (!ENV_INIT_DONE)
		return NULL;

	for (attr = &env_attribute_header; attr; attr = attr->next) {
		if (!strcmp(key, attr->key))
			return attr->value;
	}

	return NULL;
}

int bootenv_set_value(const char *key, const char *value, int creat_args_flag)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

int save_bootenv(void)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

static int is_bootenv_varible(const char *prop_name)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

static void bootenv_prop_init(const char *key, const char *value, void *cookie)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
}

char *read_sys_env(char *env, char *key, char *value)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return NULL;
}

int bootenv_property_list(void (*propfn)(const char *, const char *, void *),
		void *cookie)
{
	char name[PROP_NAME_MAX] = { 0, };
	char value[PROP_VALUE_MAX] = { 0, };
	char **env = environ;

	if (!propfn)
		return -EINVAL;

	while (*env) {
		if (read_sys_env(*env, name, value)) {
			propfn(name, value, cookie);
		}
		env++;
	}

	return 0;
}

void bootenv_props_load(void)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
}

int bootenv_init(void)
{
	char prefix[PROP_VALUE_MAX] = { 0, };

	ENV_PARTITIONS_SIZE = 0x10000;

	strcpy(prefix, "ubootenv.var");
	syslog(LOG_INFO, "[ubootenv] setenv key(ro.ubootenv.varible.prefix) value(%s)", prefix);
	setenv("ro.ubootenv.varible.prefix", prefix, 1);
	sprintf(PROFIX_UBOOTENV_VAR, "%s.", prefix);

	read_bootenv();
	bootenv_props_load();

	ENV_INIT_DONE = 1;

	return 0;
}

int bootenv_reinit(void)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

int bootenv_update(const char *name, const char *value)
{
	syslog(LOG_INFO, "[ubootenv] -- %s (%d) is not implemented\n", __func__, __LINE__);
	return 0;
}

const char *bootenv_get(const char *key)
{
	const char *variable_name;
	const char *env;

	pthread_mutex_lock(&env_lock);
	variable_name = key + strlen(PROFIX_UBOOTENV_VAR);
	env = bootenv_get_value(variable_name);
	pthread_mutex_unlock(&env_lock);

	return env;
}
