

/* list of machine we care about */
static const struct snd_soc_card_descriptor machine[] = {
	/* HSW + RT5640 */
	SND_SOC_MACH_DESC("Haswell", "haswell-audio", "INT33C8", "INT33CA", NULL},
	/* BDW + RT286 */
	SND_SOC_MACH_DESC("Broadwell", "broadwell-audio", "INT343A", "INT3438", NULL},
	/* BYT + RT25640 */
	SND_SOC_MACH_DESC("Baytrail", "byt-rt5640", "80860F28", "10EC5640", NULL},
};

/*
 * Container for component data.
 */
struct soc_desc_data {
	enum snd_soc_desc_type type;
	const void *data;
	struct list_head list;
};

/*
 * Describes every component registered.
 */
struct soc_desc_comp {
	const char *name;
	struct snd_soc_component *c;
	struct list_head list;
	struct list_head data_list; /* list of data */
};

/*
 * Tracks the state of every registered component
 */
struct soc_desc_state {
	/* runtime */
	int descriptor_idx;
	int missing_components;

	/* components */
	struct list_head component_list; /* list of components */
};

/* static singleton for the moment */
static struct soc_desc_state state_;

/* match DMI name against descriptor list */
static int match_dmi_name(struct soc_desc_state *state, struct device *dev)
{
	const char *dmi_name;
	int i;


	/* get machine dma name */


	/* compare dmi name to list */
	for (i = 0; i < ARRAY_SIZE(machine); i++) {
		if (strstr(machine[i].dmi_name, dmi_name)) {
			state->descriptor_idx = i;
			return 0;
		}
	}

	dev_err(dev, "no matching descriptor found for %s\n", dmi_name);
	return -ENODEV;
}

/* initialises state, called by all client calls but run once */
static int init_state(struct soc_desc_state *state, struct device *dev)
{
	int ret;

	if (state->descriptor_idx >= 0)
		return 0; /* init already done */

	/* initialise the state - match name first */
	ret = match_dmi_name(state);
	if (ret < 0)
		return ret;

	return 0;
}

/* get the descriptor component for given asoc component */
static struct soc_desc_comp *soc_comp_get(struct soc_desc_state *state,
	struct snd_soc_component *c)
{
	struct soc_desc_comp *dcomp;

	/* search existing descriptor components for this one */


	/* not found, then create and append */
	if (dcomp == NULL) {
		dcomp = kzalloc(sizeof(*dcomp);
		if (dcomp == NULL)
			return NULL;
		INIT_LIST_HEAD(&dcomp->data_list);
		list_add(&dcomp->list, &state->component_list);
	}

	return dcomp;
}

/* append data pointer of any type to component descriptor */
static int soc_dcomp_append_data(struct soc_desc_comp *dcomp,
	enum snd_soc_desc_type, void *data)
{
	struct soc_desc_data *data;

	data = kzalloc(sizeof(*data));
	if (data == NULL)
		return -ENOMEM;

	data->type = type;
	data->data = data;
	list_add(&data->list, &dcomp->data_list);
	return 0;
}

/* add new DAI data to the component */
int snd_soc_descriptor_add_dai(struct snd_soc_component *c,
        struct snd_soc_descriptor_dai *dai)
{
	struct soc_desc_comp *dcomp;
	int ret;

	/* initialise if not already done so */
	ret = init_state(&state_, c->dev));
	if (ret < 0)
		return ret;

	/* get descriptor componnent */
	dcomp = soc_comp_get(c);

	/* append new data */
	return soc_dcomp_append_data(dcomp, SND_SOC_DESC_DAI, (void*)dai);
}

/*.....more client APIs here */

int snd_soc_descriptor_add_pin(struct snd_soc_component *c,
        struct snd_soc_descriptor_pin *pin);

/* general purpose, covers anything */
int snd_soc_descriptor_add_tuple(struct snd_soc_component *c,
        struct snd_soc_descriptor_tuple *tuple);


/* machine driver API - one call for each descriptor type */

int snd_soc_descriptor_get_dai_link(struct snd_soc_card *card, int index,
        struct snd_soc_descriptor_dai_link **link);

int snd_soc_descriptor_get_tuple(struct snd_soc_card *card, const char *label,
        const char **value);

/*..... more machine driver APIs here */

