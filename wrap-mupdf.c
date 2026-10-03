/*
    KindlePDFViewer: MuPDF abstraction for Lua
    Copyright (C) 2011 Hans-Werner Hilse <hilse@web.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "wrap-mupdf.h"

static double LOG_TRESHOLD_PERC = 0.05; // 5%

enum {
    MAGIC = 0x3795d42b,
};

typedef struct header {
    int magic;
    size_t sz;
} header;

static size_t msize = 0U;
static size_t msize_prev;
static size_t msize_max;
static size_t msize_min;
static bool is_realloc = false;

#if 0
static size_t msize_iniz;

static void resetMsize() {
    msize_iniz = msize;
    msize_prev = 0;
    msize_max = 0;
    msize_min = (size_t) -1;
}

static void showMsize() {
    char buf[15], buf2[15], buf3[15], buf4[15];
    //printf("§§§ now: %s was: %s - min: %s - max: %s\n", readable_fs(msize, buf), readable_fs(msize_iniz, buf2), readable_fs(msize_min, buf3), readable_fs(msize_max, buf4));
    resetMsize();
}
#endif

static void log_size(char *funcName) {
    if (msize_max < msize) {
        msize_max = msize;
    }
    if (msize_min > msize) {
        msize_min = msize;
    }
    if (1==0 && abs(msize - msize_prev) > msize_prev * LOG_TRESHOLD_PERC) {
        //char buf[15], buf2[15];
        //printf("§§§ %s - total: %s (was %s)\n",funcName, readable_fs(msize,buf),readable_fs(msize_prev,buf2));
        msize_prev = msize;
    }
}

static void *
my_malloc_default(void *opaque, size_t size)
{
    struct header * h = malloc(size + sizeof(header));
    if (h == NULL) {
        return NULL;
    }

    h->magic = MAGIC;
    h->sz = size;
    msize += size + sizeof(struct header);
    if (!is_realloc) {
        log_size("alloc");
    }
    return (void *)(h + 1);
}

static void
my_free_default(void *opaque, void *ptr)
{
    fprintf(stderr, "free %p (%zu)\n", ptr, msize);
    if (ptr != NULL) {
        struct header * h = ((struct header *)ptr) - 1;
        if (h->magic != MAGIC) { /* Not allocated by us */
            fprintf(stderr, "attempt to free something that doesn't belong to us!\n");
        } else {
            msize -= h->sz + sizeof(struct header);
            free(h);
        }
    }
    if (!is_realloc) {
        log_size("free");
    }
}

static void *
my_realloc_default(void *opaque, void *old, size_t size)
{
    void * newp;
    if (old == NULL) { //practically, it's a malloc
        newp = my_malloc_default(opaque, size);
    } else {
        struct header * h = ((struct header *)old) - 1;
        if (h->magic != MAGIC) { // Not allocated by my_malloc_default
            //printf("§§§ warn: not allocated by my_malloc_default, new size: %zu\n", size);
            newp = realloc(old, size);
        } else { // malloc + free
            is_realloc = true;
            size_t oldsize = h->sz;
            //printf("realloc %zu -> %zu\n", oldsize, size);
            newp = my_malloc_default(opaque, size);
            if (NULL != newp) {
                memcpy(newp, old, oldsize < size ? oldsize : size);
                my_free_default(opaque, old);
            }
            log_size("realloc");
            is_realloc = false;
        }
    }

    return(newp);
}

fz_alloc_context my_alloc_default =
{
    NULL,
    my_malloc_default,
    my_realloc_default,
    my_free_default
};

fz_alloc_context* mupdf_get_my_alloc_context() {
    return &my_alloc_default;
}

int mupdf_get_cache_size() {
    return msize;
}

int mupdf_error_code(fz_context *ctx) {
    return ctx->error.errcode;
}
char* mupdf_error_message(fz_context *ctx) {
    return ctx->error.message;
}

fz_matrix *mupdf_fz_scale(fz_matrix *m, float sx, float sy) {
    *m = fz_scale(sx, sy);
    return m;
}

fz_matrix *mupdf_fz_translate(fz_matrix *m, float tx, float ty) {
    *m = fz_translate(tx, ty);
    return m;
}

fz_matrix *mupdf_fz_pre_rotate(fz_matrix *m, float theta) {
    *m = fz_pre_rotate(*m, theta);
    return m;
}

fz_matrix *mupdf_fz_pre_translate(fz_matrix *m, float tx, float ty) {
    *m = fz_pre_translate(*m, tx, ty);
    return m;
}

fz_rect *mupdf_fz_transform_rect(fz_rect *r, const fz_matrix *m) {
    *r = fz_transform_rect(*r, *m);
    return r;
}

fz_irect *mupdf_fz_round_rect(fz_irect *ir, const fz_rect *r) {
    *ir = fz_round_rect(*r);
    return ir;
}

fz_rect *mupdf_fz_union_rect(fz_rect *a, const fz_rect *b) {
    *a = fz_union_rect(*a, *b);
    return a;
}

fz_rect *mupdf_fz_rect_from_quad(fz_rect *r, const fz_quad *q) {
    *r = fz_rect_from_quad(*q);
    return r;
}

fz_rect *mupdf_fz_bound_page(fz_context *ctx, fz_page *page, fz_rect *r) {
    *r = fz_bound_page(ctx, page);
    return r;
}

static void transparency_mask_close_device(fz_context *ctx, fz_device *dev)
{
    fz_close_device(ctx, dev->passthrough);
}

static void transparency_mask_drop_device(fz_context *ctx, fz_device *dev)
{
    fz_drop_device(ctx, dev->passthrough);
}

static void transparency_mask_fill_image(fz_context* ctx, fz_device* dev, fz_image* img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
    if (img->mask) {
        float black = 0.f;
        fz_fill_image_mask(ctx, dev->passthrough, img->mask, ctm, fz_device_gray(ctx), &black, alpha, color_params);
    }
}

fz_device *new_transparency_mask_device(fz_context* ctx, fz_device* dev)
{
    fz_device *transparency_mask_dev = fz_new_derived_device(ctx, fz_device);
    transparency_mask_dev->passthrough = fz_keep_device(ctx, dev);
    transparency_mask_dev->close_device = transparency_mask_close_device;
    transparency_mask_dev->drop_device = transparency_mask_drop_device;
    transparency_mask_dev->fill_image = transparency_mask_fill_image;
    return transparency_mask_dev;
}

int page_has_transparency_mask(fz_context* ctx, fz_page* p)
{
    /* Other document types (EPUB, XPS, etc.) don't use smasks. */
    pdf_page* page = pdf_page_from_fz_page(ctx, p);
    if (!page) {
        return 0;
    }

    pdf_obj* resources = pdf_page_resources(ctx, page);
    pdf_obj* xobj = pdf_dict_get(ctx, resources, PDF_NAME(XObject));
    if (!xobj) {
        return 0;
    }

    static pdf_obj* const mask_keys[] = {
        PDF_NAME(SMask),
        PDF_NAME(Mask)
    };
    const int num_keys = sizeof(mask_keys) / sizeof(mask_keys[0]);

    const int n = pdf_dict_len(ctx, xobj);
    for (int i = 0; i < n; i++) {
        pdf_obj* val = pdf_dict_get_val(ctx, xobj, i);
        for (int k = 0; k < num_keys; k++) {
            if (pdf_dict_get(ctx, val, mask_keys[k]))
                return 1;
        }
    }
    return 0;
}

/* wrappers for functions that throw exceptions mupdf-style (setjmp/longjmp) */

#define MUPDF_DO_WRAP
#include "wrap-mupdf.h"


// Full fz_run_page order, replacing only the PDF annotation phase. The pointer
// array is borrowed from the caller's currently open page, never an object ID.
DLL_PUBLIC bool
mupdf_run_page_filtered(fz_context *ctx, fz_page *page, fz_device *dev,
    const fz_matrix *transform, fz_cookie *cookie, pdf_annot *const *skip, int count)
{
    fz_try(ctx)
    {
        if (!count) {
            fz_run_page(ctx, page, dev, *transform, cookie);
        } else {
            pdf_page *pdfpage = pdf_page_from_fz_page(ctx, page);
            if (!pdfpage) fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation filter requires PDF");
            fz_run_page_contents(ctx, page, dev, *transform, cookie);
            fz_try(ctx)
            {
                pdf_update_page(ctx, pdfpage);
                if (cookie && cookie->progress_max != (size_t)-1) {
                    int total = 1;
                    for (pdf_annot *a = pdf_first_annot(ctx, pdfpage); a; a = pdf_next_annot(ctx, a))
                        ++total;
                    cookie->progress_max += total;
                }
                for (pdf_annot *a = pdf_first_annot(ctx, pdfpage); a; a = pdf_next_annot(ctx, a)) {
                    if (cookie) {
                        if (cookie->abort) break;
                        ++cookie->progress;
                    }
                    bool hidden = false;
                    for (int i = 0; i < count; ++i)
                        if (a == skip[i]) { hidden = true; break; }
                    if (!hidden) pdf_run_annot(ctx, a, dev, *transform, cookie);
                }
            }
            fz_catch(ctx)
            {
                dev->close_device = NULL;
                fz_rethrow_unless(ctx, FZ_ERROR_ABORT);
                fz_ignore_error(ctx);
            }
            fz_run_page_widgets(ctx, page, dev, *transform, cookie);
        }
    }
    fz_catch(ctx) { return false; }
    return true;
}

DLL_PUBLIC bool
mupdf_pdf_annot_color_info(fz_context *ctx, pdf_annot *annot,
    int *n, float components[4], float rgb[3])
{
    fz_try(ctx)
    {
        pdf_annot_color(ctx, annot, n, components);
        if (*n != 0) {
            fz_colorspace *src = *n == 1 ? fz_device_gray(ctx) :
                *n == 3 ? fz_device_rgb(ctx) : *n == 4 ? fz_device_cmyk(ctx) : NULL;
            if (!src) fz_throw(ctx, FZ_ERROR_ARGUMENT, "unsupported annotation color");
            fz_convert_color(ctx, src, components, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);
        }
    }
    fz_catch(ctx) { return false; }
    return true;
}

DLL_PUBLIC bool
mupdf_pdf_annot_identity(fz_context *ctx, pdf_annot *annot, int *num, int *gen)
{
    fz_try(ctx)
    {
        pdf_obj *obj = pdf_annot_obj(ctx, annot);
        *num = pdf_to_num(ctx, obj);
        *gen = pdf_to_gen(ctx, obj);
    }
    fz_catch(ctx) { return false; }
    return true;
}

DLL_PUBLIC bool
mupdf_pdf_set_markup_type(fz_context *ctx, pdf_annot *annot, int type)
{
    fz_try(ctx)
    {
        if (type < PDF_ANNOT_HIGHLIGHT || type > PDF_ANNOT_STRIKE_OUT)
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "expected text markup annotation type");
        // These four subtypes have the same QuadPoints contract. Change in place
        // so third-party keys, author, dates, opacity and references survive.
        pdf_dict_put_name(ctx, pdf_annot_obj(ctx, annot), PDF_NAME(Subtype),
            pdf_string_from_annot_type(ctx, type));
        pdf_dirty_annot(ctx, annot);
    }
    fz_catch(ctx) { return false; }
    return true;
}


/* PDF annotation relationships stay within one candidate document. A reply may
 * refer to a markup on another page; a popup and a structure-parent key cannot
 * be shared because each must identify exactly one annotation. */
static void
annotation_number_tree_bounds(fz_context *ctx, pdf_obj *node, int depth, int *first, int *last)
{
    if (depth > 64 || !pdf_is_dict(ctx, node))
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid or cyclic annotation ParentTree");
    pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
    pdf_obj *nums = pdf_dict_get(ctx, node, PDF_NAME(Nums));
    int count = pdf_array_len(ctx, kids);
    *first = *last = -1;
    if (kids) {
        if (!pdf_is_array(ctx, kids) || nums || count == 0)
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid annotation ParentTree Kids");
        for (int i = 0; i < count; ++i) {
            int low, high;
            annotation_number_tree_bounds(ctx, pdf_array_get(ctx, kids, i), depth + 1, &low, &high);
            if (low < 0 || (i && low <= *last))
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "unordered annotation ParentTree Kids");
            if (i == 0) *first = low;
            *last = high;
        }
    } else {
        count = pdf_array_len(ctx, nums);
        if (!pdf_is_array(ctx, nums) || count % 2)
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid annotation ParentTree Nums");
        for (int i = 0; i < count; i += 2) {
            pdf_obj *key = pdf_array_get(ctx, nums, i);
            int value = pdf_to_int(ctx, key);
            if (!pdf_is_int(ctx, key) || value < 0 || (i && value <= *last))
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "unordered annotation ParentTree Nums");
            if (i == 0) *first = value;
            *last = value;
        }
    }
}

static void
annotation_number_tree_limits(fz_context *ctx, pdf_obj *node, int first, int last)
{
    if (first < 0) {
        pdf_dict_del(ctx, node, PDF_NAME(Limits));
    } else {
        pdf_obj *limits = pdf_dict_put_array(ctx, node, PDF_NAME(Limits), 2);
        pdf_array_push_int(ctx, limits, first);
        pdf_array_push_int(ctx, limits, last);
    }
}

/* Append a key larger than every current key. Keep existing tree objects and
 * other entries (including page marked-content arrays) intact. */
static void
annotation_number_tree_append(fz_context *ctx, pdf_obj *node, int key, pdf_obj *value)
{
    pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
    if (kids) {
        annotation_number_tree_append(ctx, pdf_array_get(ctx, kids, pdf_array_len(ctx, kids) - 1), key, value);
    } else {
        pdf_obj *nums = pdf_dict_get(ctx, node, PDF_NAME(Nums));
        pdf_array_push_int(ctx, nums, key);
        pdf_array_push(ctx, nums, value);
    }
    int first, last;
    annotation_number_tree_bounds(ctx, node, 0, &first, &last);
    annotation_number_tree_limits(ctx, node, first, last);
}

static int
annotation_number_tree_remove(fz_context *ctx, pdf_obj *node, int key)
{
    pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
    if (kids) {
        for (int i = 0; i < pdf_array_len(ctx, kids); ++i) {
            if (annotation_number_tree_remove(ctx, pdf_array_get(ctx, kids, i), key))
                pdf_array_delete(ctx, kids, i--);
        }
        if (!pdf_array_len(ctx, kids)) {
            pdf_dict_del(ctx, node, PDF_NAME(Kids));
            pdf_dict_put_array(ctx, node, PDF_NAME(Nums), 0);
        }
    } else {
        pdf_obj *nums = pdf_dict_get(ctx, node, PDF_NAME(Nums));
        for (int i = 0; i < pdf_array_len(ctx, nums); i += 2) {
            if (pdf_array_get_int(ctx, nums, i) == key) {
                pdf_array_delete(ctx, nums, i);
                pdf_array_delete(ctx, nums, i);
                break;
            }
        }
    }
    int first, last;
    annotation_number_tree_bounds(ctx, node, 0, &first, &last);
    annotation_number_tree_limits(ctx, node, first, last);
    return first < 0;
}

static int
annotation_objr_matches(fz_context *ctx, pdf_obj *kid, pdf_obj *annotation)
{
    return pdf_is_dict(ctx, kid) && !strcmp(pdf_dict_get_name(ctx, kid, PDF_NAME(Type)), "OBJR") &&
        !pdf_objcmp(ctx, pdf_dict_gets(ctx, kid, "Obj"), annotation);
}

/* Validate the bidirectional structure link before any candidate mutation. */
static pdf_obj *
annotation_structure(fz_context *ctx, pdf_document *doc, pdf_obj *annotation,
    pdf_obj **root, pdf_obj **tree, int *key)
{
    pdf_obj *parent_key = pdf_dict_get(ctx, annotation, PDF_NAME(StructParent));
    if (!parent_key) return NULL;
    if (!pdf_is_int(ctx, parent_key) || (*key = pdf_to_int(ctx, parent_key)) < 0)
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid annotation StructParent key");
    *root = pdf_dict_gets(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)), "StructTreeRoot");
    *tree = pdf_dict_get(ctx, *root, PDF_NAME(ParentTree));
    int first, last;
    annotation_number_tree_bounds(ctx, *tree, 0, &first, &last);
    pdf_obj *element = pdf_lookup_number(ctx, *tree, *key);
    if (!pdf_is_dict(ctx, element))
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation StructParent has no structure element");
    pdf_obj *kids = pdf_dict_get(ctx, element, PDF_NAME(K));
    int matches = annotation_objr_matches(ctx, kids, annotation);
    if (pdf_is_array(ctx, kids))
        for (int i = 0; i < pdf_array_len(ctx, kids); ++i)
            matches += annotation_objr_matches(ctx, pdf_array_get(ctx, kids, i), annotation);
    if (matches != 1)
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation StructParent must have one matching OBJR");
    return element;
}

static void
annotation_copy_structure(fz_context *ctx, pdf_page *page, pdf_obj *destination,
    pdf_obj *root, pdf_obj *tree, pdf_obj *element)
{
    int first, last;
    annotation_number_tree_bounds(ctx, tree, 0, &first, &last);
    int next = pdf_to_int(ctx, pdf_dict_gets(ctx, root, "ParentTreeNextKey"));
    if (last == INT_MAX || next == INT_MAX)
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation ParentTree key space exhausted");
    if (next <= last) next = last + 1;
    if (next < 0) next = 0;
    pdf_obj *old_kids = pdf_dict_get(ctx, element, PDF_NAME(K));
    pdf_obj *kids = pdf_is_array(ctx, old_kids) ? pdf_copy_array(ctx, old_kids) : pdf_new_array(ctx, page->doc, 2);
    pdf_obj *objr = NULL;
    fz_var(objr);
    fz_try(ctx) {
        if (!pdf_is_array(ctx, old_kids)) pdf_array_push(ctx, kids, old_kids);
        objr = pdf_new_dict(ctx, page->doc, 3);
        pdf_dict_put_name(ctx, objr, PDF_NAME(Type), "OBJR");
        pdf_dict_puts(ctx, objr, "Obj", destination);
        pdf_dict_puts(ctx, objr, "Pg", page->obj);
        pdf_array_push(ctx, kids, objr);
        pdf_dict_put(ctx, element, PDF_NAME(K), kids);
        annotation_number_tree_append(ctx, tree, next, element);
        pdf_dict_put_int(ctx, destination, PDF_NAME(StructParent), next);
        pdf_dict_puts_drop(ctx, root, "ParentTreeNextKey", pdf_new_int(ctx, next + 1));
    }
    fz_always(ctx) { pdf_drop_obj(ctx, kids); pdf_drop_obj(ctx, objr); }
    fz_catch(ctx) { fz_rethrow(ctx); }
}

static void
annotation_remove_structure(fz_context *ctx, pdf_document *doc, pdf_obj *original)
{
    pdf_obj *root = NULL, *tree = NULL;
    int key = -1;
    pdf_obj *element = annotation_structure(ctx, doc, original, &root, &tree, &key);
    if (!element) return;
    pdf_obj *kids = pdf_dict_get(ctx, element, PDF_NAME(K));
    if (pdf_is_array(ctx, kids)) {
        for (int i = 0; i < pdf_array_len(ctx, kids); ++i)
            if (annotation_objr_matches(ctx, pdf_array_get(ctx, kids, i), original))
                pdf_array_delete(ctx, kids, i--);
    } else {
        pdf_dict_del(ctx, element, PDF_NAME(K));
    }
    annotation_number_tree_remove(ctx, tree, key);
    pdf_dict_del(ctx, original, PDF_NAME(StructParent));
}

DLL_PUBLIC bool
mupdf_pdf_delete_annot(fz_context *ctx, pdf_page *page, pdf_annot *annot)
{
    if (!annot) return true;
    fz_try(ctx) {
        if (pdf_annot_page(ctx, annot) != page)
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation belongs to another page");
        pdf_obj *original = pdf_annot_obj(ctx, annot);
        annotation_remove_structure(ctx, page->doc, original);
        annotation_remove_structure(ctx, page->doc, pdf_dict_get(ctx, original, PDF_NAME(Popup)));
        pdf_delete_annot(ctx, page, annot);
    }
    fz_catch(ctx) { return false; }
    return true;
}

static void
annotation_validate_reply(fz_context *ctx, pdf_obj *annotation, int depth)
{
    pdf_obj *reply = pdf_dict_get(ctx, annotation, PDF_NAME(IRT));
    if (!reply) return;
    if (depth > 64 || !pdf_is_dict(ctx, reply) ||
        !pdf_is_name(ctx, pdf_dict_get(ctx, reply, PDF_NAME(Subtype))) ||
        (pdf_dict_get(ctx, reply, PDF_NAME(Type)) &&
            !pdf_name_eq(ctx, pdf_dict_get(ctx, reply, PDF_NAME(Type)), PDF_NAME(Annot))))
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid, cyclic or overly deep annotation reply chain");
    const char *kind = pdf_to_name(ctx, pdf_dict_get(ctx, annotation, PDF_NAME(RT)));
    if (*kind && strcmp(kind, "R") && strcmp(kind, "Group"))
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid annotation reply type");
    annotation_validate_reply(ctx, reply, depth + 1);
}

DLL_PUBLIC pdf_annot *
mupdf_pdf_create_annot_from(fz_context *ctx, pdf_page *page, enum pdf_annot_type type, pdf_annot *source)
{
    pdf_annot *created = NULL;
    fz_var(created);
    fz_try(ctx)
    {
        pdf_page *source_page = pdf_annot_page(ctx, source);
        if (!source_page || source_page->doc != page->doc)
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "annotation template must belong to this PDF");
        pdf_obj *original = pdf_annot_obj(ctx, source), *root = NULL, *tree = NULL;
        annotation_validate_reply(ctx, original, 0);
        int key = -1;
        pdf_obj *element = annotation_structure(ctx, page->doc, original, &root, &tree, &key);
        pdf_obj *popup = pdf_dict_get(ctx, original, PDF_NAME(Popup));
        if (popup && (!pdf_name_eq(ctx, pdf_dict_get(ctx, popup, PDF_NAME(Subtype)), PDF_NAME(Popup)) ||
            (pdf_dict_get(ctx, popup, PDF_NAME(Parent)) && pdf_objcmp(ctx, pdf_dict_get(ctx, popup, PDF_NAME(Parent)), original))))
            fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid source annotation Popup/Parent relationship");
        pdf_obj *popup_root = NULL, *popup_tree = NULL;
        int popup_key = -1;
        pdf_obj *popup_element = annotation_structure(ctx, page->doc, popup,
            &popup_root, &popup_tree, &popup_key);
        created = pdf_create_annot(ctx, page, type);
        pdf_obj *destination = pdf_annot_obj(ctx, created);
        for (int i = 0; i < pdf_dict_len(ctx, original); ++i) {
            pdf_obj *field = pdf_dict_get_key(ctx, original, i);
            const char *name = pdf_to_name(ctx, field);
            if (!strcmp(name, "P") || !strcmp(name, "Rect") || !strcmp(name, "AP") ||
                !strcmp(name, "QuadPoints") || !strcmp(name, "Subtype") || !strcmp(name, "NM") ||
                !strcmp(name, "Popup") || !strcmp(name, "StructParent"))
                continue;
            // IRT/RT refer to the same existing conversation; an unknown Parent
            // field and all other unknown keys keep their document-local refs.
            pdf_dict_put(ctx, destination, field, pdf_dict_get_val(ctx, original, i));
        }
        pdf_dict_put(ctx, destination, PDF_NAME(P), page->obj);
        if (popup) {
            pdf_set_annot_popup(ctx, created, pdf_annot_popup(ctx, source));
            pdf_obj *new_popup = pdf_dict_get(ctx, destination, PDF_NAME(Popup));
            for (int i = 0; i < pdf_dict_len(ctx, popup); ++i) {
                pdf_obj *field = pdf_dict_get_key(ctx, popup, i);
                const char *name = pdf_to_name(ctx, field);
                if (!strcmp(name, "P") || !strcmp(name, "Parent") || !strcmp(name, "Rect") ||
                    !strcmp(name, "AP") || !strcmp(name, "NM") || !strcmp(name, "StructParent")) continue;
                pdf_dict_put(ctx, new_popup, field, pdf_dict_get_val(ctx, popup, i));
            }
            pdf_dict_put(ctx, new_popup, PDF_NAME(P), page->obj);
            if (popup_element) annotation_copy_structure(ctx, page, new_popup, popup_root, popup_tree, popup_element);
        }
        if (element) annotation_copy_structure(ctx, page, destination, root, tree, element);
        pdf_dirty_annot(ctx, created);
    }
    fz_catch(ctx) { return NULL; }
    return created;
}
