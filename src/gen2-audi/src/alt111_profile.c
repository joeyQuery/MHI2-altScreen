#include "alt111.h"
#include <string.h>

const char *alt111_initial_url(void) { return "maps:/car/instrumentcluster"; }

void alt111_profile_mu1440(struct alt111_profile *p, unsigned hid_ready)
{
    memset(p, 0, sizeof(*p));
    memcpy(p->uuid, "b7e6c5a0-2222-4000-8000-000000000002", 37);
    p->type = 111;
    p->width = 1010; p->height = 376;
    p->width_mm = 200; p->height_mm = 74; p->max_fps = 30;
    p->hid_ready = hid_ready ? 1u : 0u;
    p->features = p->hid_ready ? 2u : 0u;
    p->primary_input = p->hid_ready ? 3u : 0u;
    p->view_count = 1;
    p->views[0].area.width = p->width;
    p->views[0].area.height = p->height;
    p->views[0].safe = p->views[0].area;
}

static int contains(const struct alt111_rect *outer, const struct alt111_rect *r)
{
    /* Subtraction after ordering checks avoids coordinate overflow. */
    return r->width && r->height && r->x >= outer->x && r->y >= outer->y &&
        r->x - outer->x <= outer->width && r->y - outer->y <= outer->height &&
        r->width <= outer->width - (r->x - outer->x) &&
        r->height <= outer->height - (r->y - outer->y);
}

int alt111_profile_validate(const struct alt111_profile *p)
{
    unsigned i;
    struct alt111_rect screen;
    if (!p || p->type != 111 || !p->width || !p->height || !p->width_mm ||
        !p->height_mm || !p->max_fps || p->max_fps > 60 ||
        !p->view_count || p->view_count > ALT111_MAX_VIEWS ||
        p->initial_view >= p->view_count || p->hid_ready > 1 ||
        p->draw_outside_safe > 1 || p->transition_control > 1 || p->uuid[36])
        return ALT111_INVALID;
    for (i = 0; i < 36; ++i) {
        char ch = p->uuid[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (ch != '-') return ALT111_INVALID;
        } else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                     (ch >= 'A' && ch <= 'F'))) return ALT111_INVALID;
    }
    if ((p->hid_ready && (p->features != 2 || p->primary_input != 3)) ||
        (!p->hid_ready && (p->features || p->primary_input))) return ALT111_INVALID;
    screen.x = screen.y = 0; screen.width = p->width; screen.height = p->height;
    for (i = 0; i < p->view_count; ++i)
        if (!contains(&screen, &p->views[i].area) ||
            !contains(&p->views[i].area, &p->views[i].safe)) return ALT111_INVALID;
    return ALT111_OK;
}
