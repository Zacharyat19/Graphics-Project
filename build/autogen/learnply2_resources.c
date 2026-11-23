#include <string.h>

#define RSRC_COUNT 4

#include <learnply2_autogen_resource_data.h>

typedef struct resource
{
    const char *name;
    const uint8_t *data;
    const uint32_t *size;
} resource_t;

static const resource_t resources[] = {
{"shaders/silhouette.vert", silhouette_vert, &silhouette_vert_size},
{"shaders/silhouette.frag", silhouette_frag, &silhouette_frag_size},
{"shaders/silhouette.geom", silhouette_geom, &silhouette_geom_size},
{"shaders/silhouette_120.geom", silhouette_120_geom, &silhouette_120_geom_size},
};

const uint8_t *learnply2_find_resource(const char *name, uint32_t *size)
{
    const resource_t *r;
    for (r = resources; r < resources + RSRC_COUNT; ++r)
    {
        if (!strcmp(name, r->name))
        {
            if (size)
                *size = *r->size;
            return r->data;
        }
    }
    return NULL;
}
