/* Copyright (C) 2001-2022 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Accumulator for clipping paths */
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gsdcolor.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxgstate.h"
#include "gzpath.h"
#include "gxpaint.h"
#include "gzcpath.h"
#include "gzacpath.h"
#include "gxdevsop.h"

/* Device procedures */
static dev_proc_open_device(accum_open_device);
static dev_proc_close_device(accum_close);
static dev_proc_fill_rectangle(accum_fill_rectangle);
static dev_proc_dev_spec_op(accum_dev_spec_op);
static dev_proc_get_clipping_box(accum_get_clipping_box);

/* GC information */
extern_st(st_clip_list);
static
ENUM_PTRS_WITH(device_cpath_accum_enum_ptrs, gx_device_cpath_accum *pdev)
    if (index >= st_device_max_ptrs)
        return ENUM_USING(st_clip_list, &pdev->list, sizeof(gx_clip_list), index - st_device_max_ptrs);
    ENUM_PREFIX(st_device, 0);
ENUM_PTRS_END
static
RELOC_PTRS_WITH(device_cpath_accum_reloc_ptrs, gx_device_cpath_accum *pdev)
{   RELOC_PREFIX(st_device);
    RELOC_USING(st_clip_list, &pdev->list, size);
} RELOC_PTRS_END

public_st_device_cpath_accum();

/* The device descriptor */
static void
cpath_accum_initialize_device_procs(gx_device *dev)
{
    set_dev_proc(dev, open_device, accum_open_device);
    set_dev_proc(dev, close_device, accum_close);
    set_dev_proc(dev, fill_rectangle, accum_fill_rectangle);
    set_dev_proc(dev, get_clipping_box, accum_get_clipping_box);
    set_dev_proc(dev, get_color_mapping_procs, gx_default_DevGray_get_color_mapping_procs);
    set_dev_proc(dev, dev_spec_op, accum_dev_spec_op);
}


/* Many of these procedures won't be called; they are set to NULL. */
static const gx_device_cpath_accum gs_cpath_accum_device =
{std_device_std_body(gx_device_cpath_accum,
                     cpath_accum_initialize_device_procs,
                     "clip list accumulator",
                     0, 0, 1, 1)
};

/* Start accumulating a clipping path. */
void
gx_cpath_accum_begin(gx_device_cpath_accum * padev, gs_memory_t * mem, bool transpose)
{
    gx_device_init_on_stack((gx_device *) padev,
                            (const gx_device *) & gs_cpath_accum_device, mem);
    padev->list_memory = mem;
    set_dev_proc(padev, encode_color, gx_default_gray_encode);
    set_dev_proc(padev, decode_color, gx_default_decode_color);
    (*dev_proc(padev, open_device)) ((gx_device *) padev);
    padev->list.transpose = transpose;
}

void
gx_cpath_accum_set_cbox(gx_device_cpath_accum * padev,
                        const gs_fixed_rect * pbox)
{
    /* fixed2int_var_ceiling(x) overflows for anything larger
     * than max_fixed - fixed_scale - 1. So to protect against
     * us doing bad things when passed a min_fixed/max_fixed box,
     * clip appropriately. */
    fixed upperx = pbox->q.x;
    fixed uppery = pbox->q.y;
    if (upperx > max_fixed - fixed_scale - 1)
        upperx = max_fixed - fixed_scale - 1;
    if (uppery > max_fixed - fixed_scale - 1)
        uppery = max_fixed - fixed_scale - 1;
    if (padev->list.transpose) {
        padev->clip_box.p.x = fixed2int_var(pbox->p.y);
        padev->clip_box.p.y = fixed2int_var(pbox->p.x);
        padev->clip_box.q.x = fixed2int_var_ceiling(uppery);
        padev->clip_box.q.y = fixed2int_var_ceiling(upperx);
    } else {
        padev->clip_box.p.x = fixed2int_var(pbox->p.x);
        padev->clip_box.p.y = fixed2int_var(pbox->p.y);
        padev->clip_box.q.x = fixed2int_var_ceiling(upperx);
        padev->clip_box.q.y = fixed2int_var_ceiling(uppery);
    }
}

static void
accum_get_clipping_box(gx_device *dev, gs_fixed_rect *pbox)
{
    gx_device_cpath_accum * padev = (gx_device_cpath_accum *)dev;

    if (padev->list.transpose) {
        pbox->p.x = int2fixed(padev->clip_box.p.y);
        pbox->p.y = int2fixed(padev->clip_box.p.x);
        pbox->q.x = int2fixed(padev->clip_box.q.y+1)-1;
        pbox->q.y = int2fixed(padev->clip_box.q.x+1)-1;
    } else {
        pbox->p.x = int2fixed(padev->clip_box.p.x);
        pbox->p.y = int2fixed(padev->clip_box.p.y);
        pbox->q.x = int2fixed(padev->clip_box.q.x+1)-1;
        pbox->q.y = int2fixed(padev->clip_box.q.y+1)-1;
    }
}

/* Finish accumulating a clipping path. */
/* NB: After this the padev bbox will be restored to "normal" untransposed */
int
gx_cpath_accum_end(gx_device_cpath_accum * padev, gx_clip_path * pcpath)
{
    int code = (*dev_proc(padev, close_device)) ((gx_device *) padev);
    /* Make an entire clipping path so we can use cpath_assign. */
    gx_clip_path apath;

    if (code < 0)
        return code;
    gx_cpath_init_local(&apath, padev->list_memory);
    apath.rect_list->list = padev->list;
    if (padev->list.count == 0)
        apath.path.bbox.p.x = apath.path.bbox.p.y =
        apath.path.bbox.q.x = apath.path.bbox.q.y = 0;
    else {
        if (padev->list.transpose) {
            int tmp;

            tmp = padev->bbox.p.x;
            padev->bbox.p.x = padev->bbox.p.y;
            padev->bbox.p.y = tmp;
            tmp = padev->bbox.q.x;
            padev->bbox.q.x = padev->bbox.q.y;
            padev->bbox.q.y = tmp;
        }
        apath.path.bbox.p.x = int2fixed(padev->bbox.p.x);
        apath.path.bbox.p.y = int2fixed(padev->bbox.p.y);
        apath.path.bbox.q.x = int2fixed(padev->bbox.q.x);
        apath.path.bbox.q.y = int2fixed(padev->bbox.q.y);
    }
    /* indicate that the bbox is accurate */
    apath.path.bbox_accurate = 1;
    /* Note that the result of the intersection might be */
    /* a single rectangle.  This will cause clip_path_is_rect.. */
    /* to return true.  This, in turn, requires that */
    /* we set apath.inner_box correctly. */
    if (clip_list_is_rectangle(&padev->list))
        apath.inner_box = apath.path.bbox;
    else {
        /* The quick check must fail. */
        apath.inner_box.p.x = apath.inner_box.p.y = 0;
        apath.inner_box.q.x = apath.inner_box.q.y = 0;
    }
    gx_cpath_set_outer_box(&apath);
    apath.path_valid = false;
    apath.id = gs_next_ids(padev->list_memory, 1);	/* path changed => change id */
    apath.cached = NULL;
    code = gx_cpath_assign_free(pcpath, &apath);
    return code;
}

/* Discard an accumulator in case of error. */
void
gx_cpath_accum_discard(gx_device_cpath_accum * padev)
{
    gx_clip_list_free(&padev->list, padev->list_memory);
}

/* Intersect two clipping paths using an accumulator. */
int
gx_cpath_intersect_path_slow(gx_clip_path * pcpath, gx_path * ppath,
                             int rule, gs_gstate *pgs,
                             const gx_fill_params * params0)
{
    gs_logical_operation_t save_lop = gs_current_logical_op_inline(pgs);
    gx_device_cpath_accum adev;
    gx_device_color devc;
    gx_fill_params params;
    int code;

    gx_cpath_accum_begin(&adev, pcpath->path.memory, false);
    set_nonclient_dev_color(&devc, 0);	/* arbitrary, but not transparent */
    gs_set_logical_op_inline(pgs, lop_default);
    if (params0 != 0)
        params = *params0;
    else {
        gs_point fadjust;
        params.rule = rule;
        gs_currentfilladjust(pgs, &fadjust);
        params.adjust.x = float2fixed(fadjust.x);
        params.adjust.y = float2fixed(fadjust.y);
        params.flatness = gs_currentflat_inline(pgs);
    }
    code = gx_fill_path_only(ppath, (gx_device *)&adev, pgs,
                             &params, &devc, pcpath);
    if (code < 0 || (code = gx_cpath_accum_end(&adev, pcpath)) < 0)
        gx_cpath_accum_discard(&adev);
    gs_set_logical_op_inline(pgs, save_lop);
    return code;
}

/* ------ Device implementation ------ */

#ifdef DEBUG
/* Validate a clipping path after accumulation. */
static bool
clip_list_validate(const gx_clip_list * clp)
{
    if (clp->count <= 1)
        return (clp->head == 0 && clp->tail == 0 &&
                clp->single.next == 0 && clp->single.prev == 0);
    else {
        const gx_clip_rect *prev = clp->head;
        const gx_clip_rect *ptr;
        bool ok = true;

        while ((ptr = prev->next) != 0) {
            if (ptr->ymin > ptr->ymax || ptr->xmin > ptr->xmax ||
                !(ptr->ymin >= prev->ymax ||
                  (ptr->ymin == prev->ymin &&
                   ptr->ymax == prev->ymax &&
                   ptr->xmin >= prev->xmax)) ||
                ptr->prev != prev
                ) {
                clip_rect_print('q', "WRONG:", ptr);
                ok = false;
            }
            prev = ptr;
        }
        return ok && prev == clp->tail;
    }
}
#endif /* DEBUG */

/* Initialize the accumulation device. */
int
accum_open_device(register gx_device * dev)
{
    gx_device_cpath_accum * const adev = (gx_device_cpath_accum *)dev;

    gx_clip_list_init(&adev->list);
    adev->bbox.p.x = adev->bbox.p.y = fixed2int(max_fixed);
    adev->bbox.q.x = adev->bbox.q.y = fixed2int(min_fixed);
    adev->clip_box.p.x = adev->clip_box.p.y = fixed2int(min_fixed);
    adev->clip_box.q.x = adev->clip_box.q.y = fixed2int(max_fixed);
    return 0;
}

/* Close the accumulation device. */
static int
accum_close(gx_device * dev)
{
    gx_device_cpath_accum * const adev = (gx_device_cpath_accum *)dev;

    if (adev->list.transpose) {
        adev->list.xmin = adev->bbox.p.y;
        adev->list.xmax = adev->bbox.q.y;
    } else {
        adev->list.xmin = adev->bbox.p.x;
        adev->list.xmax = adev->bbox.q.x;
    }
#ifdef DEBUG
    if (gs_debug_c('q')) {
        gx_clip_rect *rp =
            (adev->list.count <= 1 ? &adev->list.single : adev->list.head);

        dmlprintf6(dev->memory,
                   "[q]list at "PRI_INTPTR", count=%d, head="PRI_INTPTR", tail="PRI_INTPTR", xrange=(%d,%d):\n",
                   (intptr_t)&adev->list, adev->list.count,
                   (intptr_t)adev->list.head, (intptr_t)adev->list.tail,
                   adev->list.xmin, adev->list.xmax);
        while (rp != 0) {
            clip_rect_print('q', "   ", rp);
            rp = rp->next;
        }
    }
    if (!clip_list_validate(&adev->list)) {
        mlprintf1(dev->memory, "[q]Bad clip list "PRI_INTPTR"!\n", (intptr_t)&adev->list);
        return_error(gs_error_Fatal);
    }
#endif
    return 0;
}

/*
   The pattern management device method.
   See gxdevcli.h about return codes.
 */
int
accum_dev_spec_op(gx_device *pdev1, int dev_spec_op,
                void *data, int size)
{
    switch (dev_spec_op) {
        case gxdso_pattern_is_cpath_accum:
            return 1;
        case gxdso_pattern_can_accum:
        case gxdso_pattern_start_accum:
        case gxdso_pattern_finish_accum:
        case gxdso_pattern_load:
        case gxdso_pattern_shading_area:
        case gxdso_pattern_shfill_doesnt_need_path:
        case gxdso_pattern_handles_clip_path:
            return 0;
    }
    return  gx_default_dev_spec_op(pdev1, dev_spec_op, data, size);
}

/* Accumulate one rectangle. */
/* Allocate a rectangle to be added to the list. */
static const gx_clip_rect clip_head_rect = {
    0, 0, min_int, min_int, min_int, min_int
};
static const gx_clip_rect clip_tail_rect = {
    0, 0, max_int, max_int, max_int, max_int
};
static gx_clip_rect *
accum_alloc_rect(gx_device_cpath_accum * adev)
{
    gs_memory_t *mem = adev->list_memory;
    gx_clip_rect *ar = gs_alloc_struct(mem, gx_clip_rect, &st_clip_rect,
                                       "accum_alloc_rect");

    if (ar == 0)
        return 0;
    if (adev->list.count == 2) {
        /* We're switching from a single rectangle to a list. */
        /* Allocate the head and tail entries. */
        gx_clip_rect *head = ar;
        gx_clip_rect *tail =
            gs_alloc_struct(mem, gx_clip_rect, &st_clip_rect,
                            "accum_alloc_rect(tail)");
        gx_clip_rect *single =
            gs_alloc_struct(mem, gx_clip_rect, &st_clip_rect,
                            "accum_alloc_rect(single)");

        ar = gs_alloc_struct(mem, gx_clip_rect, &st_clip_rect,
                             "accum_alloc_rect(head)");
        if (tail == 0 || single == 0 || ar == 0) {
            gs_free_object(mem, ar, "accum_alloc_rect");
            gs_free_object(mem, single, "accum_alloc_rect(single)");
            gs_free_object(mem, tail, "accum_alloc_rect(tail)");
            gs_free_object(mem, head, "accum_alloc_rect(head)");
            return 0;
        }
        *head = clip_head_rect;
        head->next = single;
        *single = adev->list.single;
        single->prev = head;
        single->next = tail;
        *tail = clip_tail_rect;
        tail->prev = single;
        adev->list.head = head;
        adev->list.tail = tail;
        adev->list.insert = tail;
    }
    return ar;
}
#define ACCUM_ALLOC(s, ar, px, py, qx, qy)\
        if (++(adev->list.count) == 1)\
          ar = &adev->list.single;\
        ar = accum_alloc_rect(adev);\
        if (ar) ACCUM_SET(s, ar, px, py, qx, qy)
#define ACCUM_SET(s, ar, px, py, qx, qy)\
        (ar)->xmin = px, (ar)->ymin = py, (ar)->xmax = qx, (ar)->ymax = qy;\
        clip_rect_print('Q', s, ar)
/* Link or unlink a rectangle in the list. */
#define ACCUM_ADD_LAST(ar)\
        ACCUM_ADD_BEFORE(ar, adev->list.tail)
#define ACCUM_ADD_AFTER(ar, rprev)\
        ar->prev = (rprev), (ar->next = (rprev)->next)->prev = ar,\
          (rprev)->next = ar
#define ACCUM_ADD_BEFORE(ar, rnext)\
        (ar->prev = (rnext)->prev)->next = ar, ar->next = (rnext),\
          (rnext)->prev = ar
#define ACCUM_REMOVE(ar)\
        ar->next->prev = ar->prev, ar->prev->next = ar->next
/* Free a rectangle that was removed from the list. */
#define ACCUM_FREE(s, ar)\
        if (--(adev->list.count)) {\
          clip_rect_print('Q', s, ar);\
          gs_free_object(adev->list_memory, ar, "accum_rect");\
        }
/*
 * Add a rectangle to the list.  It would be wonderful if rectangles
 * were always disjoint and always presented in the correct order,
 * but they aren't: the fill loop works by trapezoids, not by scan lines,
 * and may produce slightly overlapping rectangles because of "fattening".
 * All we can count on is that they are approximately disjoint and
 * approximately in order.
 *
 * Because of the way the fill loop handles a path that is just a single
 * rectangle, we take special care to merge Y-adjacent rectangles when
 * this is possible.
 */
static int
accum_fill_rectangle(gx_device * dev, int xi, int yi, int w, int h,
                     gx_color_index color)
{
    gx_device_cpath_accum * const adev = (gx_device_cpath_accum *)dev;
    int x, y, xe, ye;
    gx_clip_rect *nr;
    gx_clip_rect *ar;
    register gx_clip_rect *rptr;
    int ymin, ymax;

    if (adev->list.transpose) {
        x = yi, xe = yi + h;
        y = xi, ye = xi + w;
    } else {
        x = xi, xe = x + w;
        y = yi, ye = y + h;
    }

    /* Clip the rectangle being added. */
    if (y < adev->clip_box.p.y)
        y = adev->clip_box.p.y;
    if (ye > adev->clip_box.q.y)
        ye = adev->clip_box.q.y;
    if (y >= ye)
        return 0;
    if (x < adev->clip_box.p.x)
        x = adev->clip_box.p.x;
    if (xe > adev->clip_box.q.x)
        xe = adev->clip_box.q.x;
    if (x >= xe)
        return 0;

    /* Update the bounding box. */
    if (x < adev->bbox.p.x)
        adev->bbox.p.x = x;
    if (y < adev->bbox.p.y)
        adev->bbox.p.y = y;
    if (xe > adev->bbox.q.x)
        adev->bbox.q.x = xe;
    if (ye > adev->bbox.q.y)
        adev->bbox.q.y = ye;

top:
    if (adev->list.count == 0) {	/* very first rectangle */
        adev->list.count = 1;
        ACCUM_SET("single", &adev->list.single, x, y, xe, ye);
        return 0;
    }
    if (adev->list.count == 1) {	/* check for Y merging */
        rptr = &adev->list.single;
        if (x == rptr->xmin && xe == rptr->xmax &&
            y <= rptr->ymax && ye >= rptr->ymin
            ) {
            if (y < rptr->ymin)
                rptr->ymin = y;
            if (ye > rptr->ymax)
                rptr->ymax = ye;
            return 0;
        }
    }
    else
        rptr = adev->list.tail->prev;
    if (y >= rptr->ymax) {
        if (y == rptr->ymax && x == rptr->xmin && xe == rptr->xmax &&
            (rptr->prev == 0 || y != rptr->prev->ymax)
            ) {
            rptr->ymax = ye;
            return 0;
        }
        ACCUM_ALLOC("app.y", nr, x, y, xe, ye);
        if (!nr) return_error(gs_error_VMerror);
        ACCUM_ADD_LAST(nr);
        return 0;
    } else if (y == rptr->ymin && ye == rptr->ymax && x >= rptr->xmin) {
        if (x <= rptr->xmax) {
            if (xe > rptr->xmax)
                rptr->xmax = xe;
            return 0;
        }
        ACCUM_ALLOC("app.x", nr, x, y, xe, ye);
        if (!nr) return_error(gs_error_VMerror);
        ACCUM_ADD_LAST(nr);
        return 0;
    }
    ACCUM_ALLOC("accum", nr, x, y, xe, ye);
    if (!nr) return_error(gs_error_VMerror);
    /* Previously we used to always search back from the tail here. Now we
     * base our search on the previous insertion point, in the hopes that
     * locality of reference will save us time. */
    rptr = adev->list.insert->prev;
    /* We want to find the value of rptr nearest the tail, s.t.
     * ye > rptr->ymin */
    if (ye <= rptr->ymin) {
        /* Work backwards till we find the insertion point. */
        do {
            rptr = rptr->prev;
        } while (ye <= rptr->ymin);
    } else {
        /* Search forwards */
        do {
            rptr = rptr->next;
        } while (ye > rptr->ymin);
        /* And we've gone one too far */
        rptr = rptr->prev;
    }
    ymin = rptr->ymin;
    ymax = rptr->ymax;
    if (ye > ymax) {
        if (y >= ymax) {	/* Insert between two bands. */
            ACCUM_ADD_AFTER(nr, rptr);
            adev->list.insert = nr;
            return 0;
        }
        /* Split off the top part of the new rectangle. */
        ACCUM_ALLOC("a.top", ar, x, ymax, xe, ye);
        if (!ar) {
            if (nr != &adev->list.single) ACCUM_FREE("free", nr);
            return_error(gs_error_VMerror);
        }
        ACCUM_ADD_AFTER(ar, rptr);
        ye = nr->ymax = ymax;
        clip_rect_print('Q', " ymax", nr);
    }
    /* Here we know ymin < ye <= ymax; */
    /* rptr points to the last node with this value of ymin/ymax. */
    /* If necessary, split off the part of the existing band */
    /* that is above the new band. */
    if (ye < ymax) {
        gx_clip_rect *rsplit = rptr;

        while (rsplit->ymax == ymax) {
            ACCUM_ALLOC("s.top", ar, rsplit->xmin, ye, rsplit->xmax, ymax);
            if (!ar) {
                if (nr != &adev->list.single) ACCUM_FREE("free", nr);
                return_error(gs_error_VMerror);
            }
            ACCUM_ADD_AFTER(ar, rptr);
            rsplit->ymax = ye;
            rsplit = rsplit->prev;
        }
    }
    /* Now ye = ymax.  If necessary, split off the part of the */
    /* existing band that is below the new band. */
    if (y > ymin) {
        gx_clip_rect *rbot = rptr, *rsplit;

        while (rbot->prev->ymin == ymin)
            rbot = rbot->prev;
        for (rsplit = rbot;;) {
            ACCUM_ALLOC("s.bot", ar, rsplit->xmin, ymin, rsplit->xmax, y);
            if (!ar) {
                if (nr != &adev->list.single) ACCUM_FREE("free", nr);
                return_error(gs_error_VMerror);
            }
            ACCUM_ADD_BEFORE(ar, rbot);
            rsplit->ymin = y;
            if (rsplit == rptr)
                break;
            rsplit = rsplit->next;
        }
        ymin = y;
    }
    /* Now y <= ymin as well.  (y < ymin is possible.) */
    nr->ymin = ymin;
    /* Search for the X insertion point. */
    for (; rptr->ymin == ymin; rptr = rptr->prev) {
        if (xe < rptr->xmin)
            continue;		/* still too far to right */
        if (x > rptr->xmax)
            break;		/* disjoint */
        /* The new rectangle overlaps an existing one.  Merge them. */
        if (xe > rptr->xmax) {
            rptr->xmax = nr->xmax;	/* might be > xe if */
            /* we already did a merge */
            clip_rect_print('Q', "widen", rptr);
        }
        ACCUM_FREE("free", nr);
        if (x >= rptr->xmin) {
            adev->list.insert = rptr;
            goto out;
        }
        /* Might overlap other rectangles to the left. */
        rptr->xmin = x;
        nr = rptr;
        ACCUM_REMOVE(rptr);
        clip_rect_print('Q', "merge", nr);
    }
    ACCUM_ADD_AFTER(nr, rptr);
    adev->list.insert = nr;
out:
    /* Check whether there are only 0 or 1 rectangles left. */
    if (adev->list.count <= 1) {
        /* We're switching from a list to at most 1 rectangle. */
        /* Free the head and tail entries. */
        gs_memory_t *mem = adev->list_memory;
        gx_clip_rect *single = adev->list.head->next;

        if (single != adev->list.tail) {
            adev->list.single = *single;
            gs_free_object(mem, single, "accum_free_rect(single)");
            adev->list.single.next = adev->list.single.prev = 0;
        }
        gs_free_object(mem, adev->list.tail, "accum_free_rect(tail)");
        gs_free_object(mem, adev->list.head, "accum_free_rect(head)");
        adev->list.head = 0;
        adev->list.tail = 0;
        adev->list.insert = 0;
    }
    /* Check whether there is still more of the new band to process. */
    if (y < ymin) {
        /* Continue with the bottom part of the new rectangle. */
        clip_rect_print('Q', " ymin", nr);
        ye = ymin;
        goto top;
    }
    return 0;
}