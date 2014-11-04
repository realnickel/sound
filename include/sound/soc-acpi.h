/*
 * Descriptor structures that align with ACPI spec for audio (by Rafal/Vinod/Pierre).
 * They use ASoC values though for enums, it's upto client driver to do any conversion
 * for ambiguous values.
 */


/* Vinod, for your intern to complete structures based on slides :) */

/*
 * We have an internal enum ID for every ACPI descriptor structure type.
 */
enum snd_soc_desc_type {
	SND_SOC_DESC_DAI = 0,
	/* etc */
};

enum snd_soc_desc_dai_type {
	SND_SOC_DESC_DAI_HDA = 0,
	SND_SOC_DESC_DAI_RESERVED = 1,
	SND_SOC_DESC_DAI_PDM = 2,
	SND_SOC_DESC_DAI_PCM = 3, // lrg - changed from SSP
	SND_SOC_DESC_DAI_SLIMBUS = 4,
};

enum snd_soc_desc_dai_direction {
	SND_SOC_DESC_DAI_DIR_PLAYBACK = 0, // lrg - changed from render
	SND_SOC_DESC_DAI_DIR_CAPTURE = 1,
	SND_SOC_DESC_DAI_DIR_BIDIRECTIONAL = 2,
};

enum mode {
	master = 0,
	slave = 1,
};

enum protocols {
	i2s = 0,
	tdm = 1,
	pcm = 2,
	pdm = 3,
};

enum polarity {
	low = 0,
	high = 1,
};

/*
 * PCM config
 */
struct snd_soc_desc_pcm_config {
	u16	format_tag;
	u16	channel;
	u32	sample_per_second;
	u32	byte_per_second;
	u16	block_allign;
	u16	bits_per_sample;
	u16	size;
	u16	valid_bit_per_sample;
	u32	channel_mask;
	char	sub_format[16];
}__attribute__((packed, aligned(1)));	

struct specific_config {
	u32 	capabilities_size;
	u8	*capabilities;
}__attribute__((packed, aligned(1)));

struct format_config {
	struct wave_format_extensible	format;
	struct specific_config		format_config;
}__attribute__((packed, aligned(1)));


struct formats_config {
	u8	forma_config_count;
	struct format_config	*format_configs;
}__attribute__((packed, aligned(1)));




struct nhlt_endpoint_descriptor {
	u32	endpoint_descriptor_length;
	u16	deviceId;
        u8	linktype; /* enum linktypes */
	u8	virtualbusid;
	u8	direction; /* enum directions */
	struct specific_config	endpoint_config;
	struct formats_config	format_configs;
}__attribute__((packed, aligned(1)));


/*
 * HW DAI Link Config
 */

struct link_config {
	u8	link_config_name[16];
	u8	codec_port[4];
	u8	clock_mode; /* enum mode */
	u8	frame_mode; /* enum mode */
	u8	protocol;   /* enum protocols */
	u8	frame_polarity; /* enum polarity */
	u8	reserved[2];
	u32	frame_width;
	u32	frame_rate;
	u8	data_polarity; /* enum polarity */
	u8	tdm_slots;
	u8	bit_per_slots;
	u8	start_delay;
	u32	active_tx_slots;
	u32	active_rx_slots;
}__attribute__((packed, aligned(1)));

struct specific_config {
	u32	link_config_count;
	struct	link_config	*link_configs;
}__attribute__((packed, aligned(1)));


struct clt_link_descriptor {
	u32	link_descriptor_length;
	u8	linktype; /* enum linktypes */
	u8 	virtual_bus_id;
	struct specific_config	link_capabilities;
}__attribute__((packed, aligned(1)));

/* just an example for dai */
struct snd_soc_descriptor_dai {
        /* format, clock masters etc */
};

/* just an example for dai link */
struct snd_soc_descriptor_dai_link {
        ....
};

/* just an example for pin */
struct snd_soc_descriptor_pin {
        ....
};

/* more descriptor structures here */


/* descriptor tuple - shall we just make label/value char arrays
* if we have fixed sizes in ACPI table ?? */
struct snd_soc_descriptor_tuple {
        const char *label;
        const char *value;
};


/* client component driver API - called by codec, platform drivers */

/* lets use the new component structure for handle, if it's not ready upstream 
 * we can help or use existing codec, platform varients */

/* we have snd_soc_descriptor_add_() functions for each descriptor structure */

int snd_soc_descriptor_add_dai(struct snd_soc_component *c,
        struct snd_soc_descriptor_dai *dai);

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


/* core API - used by core to register machines */

/* this structure can be used to define a custom machine driver if one is needed
 * otherwise a deafult machine is used - maybe use componnent instead of
 * codec, platform paradigms */

struct snd_soc_card_descriptor {
        const char *dmi_name; /* the DMI machine name read from ACPI */
	const char *machine_drv; /* optional mach driver to invoke */
        const char *component[]; /* NULL terminated list of components */
};

/* convenience constructor for machines */
#define SND_SOC_MACH_DESC(dname, dmachine, ...) \
	{.dmi_name = dname, .machine_drv = dmachine, .components = __VA_ARGS__)

