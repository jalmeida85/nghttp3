/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2013 nghttp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nghttp3_frame.h"

#include <string.h>

#include "nghttp3_conv.h"
#include "nghttp3_str.h"

uint8_t *nghttp3_frame_write_hd(uint8_t *p, const nghttp3_frame_hd *hd) {
  p = nghttp3_put_varint(p, hd->type);
  p = nghttp3_put_varint(p, hd->length);
  return p;
}

size_t nghttp3_frame_write_hd_len(const nghttp3_frame_hd *hd) {
  return nghttp3_put_varint_len(hd->type) + nghttp3_put_varint_len(hd->length);
}

uint8_t *nghttp3_frame_write_settings(uint8_t *p,
                                      const nghttp3_frame_settings *fr) {
  size_t i;

  p = nghttp3_frame_write_hd(p, &fr->hd);

  for (i = 0; i < fr->niv; ++i) {
    p = nghttp3_put_varint(p, (int64_t)fr->iv[i].id);
    p = nghttp3_put_varint(p, (int64_t)fr->iv[i].value);
  }

  return p;
}

size_t nghttp3_frame_write_settings_len(int64_t *ppayloadlen,
                                        const nghttp3_frame_settings *fr) {
  size_t payloadlen = 0;
  size_t i;

  for (i = 0; i < fr->niv; ++i) {
    payloadlen += nghttp3_put_varint_len((int64_t)fr->iv[i].id) +
                  nghttp3_put_varint_len((int64_t)fr->iv[i].value);
  }

  *ppayloadlen = (int64_t)payloadlen;

  return nghttp3_put_varint_len(NGHTTP3_FRAME_SETTINGS) +
         nghttp3_put_varint_len((int64_t)payloadlen) + payloadlen;
}

uint8_t *nghttp3_frame_write_priority(uint8_t *p,
                                      const nghttp3_frame_priority *fr) {
  p = nghttp3_frame_write_hd(p, &fr->hd);

  *p++ =
      (uint8_t)((fr->pt << 6) | (fr->dt << 4) | (uint8_t)(fr->exclusive << 3));
  p = nghttp3_put_varint(p, fr->pri_elem_id);
  if (fr->dt != NGHTTP3_ELEM_DEP_TYPE_ROOT) {
    p = nghttp3_put_varint(p, fr->elem_dep_id);
  }
  *p++ = (uint8_t)(fr->weight - 1);

  return p;
}

size_t nghttp3_frame_write_priority_len(int64_t *ppayloadlen,
                                        const nghttp3_frame_priority *fr) {
  size_t payloadlen = 2 + nghttp3_put_varint_len(fr->pri_elem_id);

  switch (fr->dt) {
  case NGHTTP3_ELEM_DEP_TYPE_REQUEST:
  case NGHTTP3_ELEM_DEP_TYPE_PUSH:
  case NGHTTP3_ELEM_DEP_TYPE_PLACEHOLDER:
    payloadlen += nghttp3_put_varint_len(fr->elem_dep_id);
    break;
  case NGHTTP3_ELEM_DEP_TYPE_ROOT:
    break;
  }

  *ppayloadlen = (int64_t)payloadlen;

  return nghttp3_put_varint_len(NGHTTP3_FRAME_PRIORITY) +
         nghttp3_put_varint_len((int64_t)payloadlen) + payloadlen;
}

uint8_t *nghttp3_frame_write_cancel_push(uint8_t *p,
                                         const nghttp3_frame_cancel_push *fr) {
  p = nghttp3_frame_write_hd(p, &fr->hd);
  p = nghttp3_put_varint(p, fr->push_id);

  return p;
}

size_t
nghttp3_frame_write_cancel_push_len(int64_t *ppayloadlen,
                                    const nghttp3_frame_cancel_push *fr) {
  size_t payloadlen = nghttp3_put_varint_len(fr->push_id);

  *ppayloadlen = (int64_t)payloadlen;

  return nghttp3_put_varint_len(NGHTTP3_FRAME_CANCEL_PUSH) +
         nghttp3_put_varint_len((int64_t)payloadlen) + payloadlen;
}

uint8_t *nghttp3_frame_write_max_push_id(uint8_t *p,
                                         const nghttp3_frame_max_push_id *fr) {
  p = nghttp3_frame_write_hd(p, &fr->hd);
  p = nghttp3_put_varint(p, fr->push_id);

  return p;
}

size_t
nghttp3_frame_write_max_push_id_len(int64_t *ppayloadlen,
                                    const nghttp3_frame_max_push_id *fr) {
  size_t payloadlen = nghttp3_put_varint_len(fr->push_id);

  *ppayloadlen = (int64_t)payloadlen;

  return nghttp3_put_varint_len(NGHTTP3_FRAME_MAX_PUSH_ID) +
         nghttp3_put_varint_len((int64_t)payloadlen) + payloadlen;
}

int nghttp3_nva_copy(nghttp3_nv **pnva, const nghttp3_nv *nva, size_t nvlen,
                     const nghttp3_mem *mem) {
  size_t i;
  uint8_t *data = NULL;
  size_t buflen = 0;
  nghttp3_nv *p;

  if (nvlen == 0) {
    *pnva = NULL;

    return 0;
  }

  for (i = 0; i < nvlen; ++i) {
    /* + 1 for null-termination */
    if ((nva[i].flags & NGHTTP3_NV_FLAG_NO_COPY_NAME) == 0) {
      buflen += nva[i].namelen + 1;
    }
    if ((nva[i].flags & NGHTTP3_NV_FLAG_NO_COPY_VALUE) == 0) {
      buflen += nva[i].valuelen + 1;
    }
  }

  buflen += sizeof(nghttp3_nv) * nvlen;

  *pnva = nghttp3_mem_malloc(mem, buflen);

  if (*pnva == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  p = *pnva;
  data = (uint8_t *)(*pnva) + sizeof(nghttp3_nv) * nvlen;

  for (i = 0; i < nvlen; ++i) {
    p->flags = nva[i].flags;

    if (nva[i].flags & NGHTTP3_NV_FLAG_NO_COPY_NAME) {
      p->name = nva[i].name;
      p->namelen = nva[i].namelen;
    } else {
      if (nva[i].namelen) {
        memcpy(data, nva[i].name, nva[i].namelen);
      }
      p->name = data;
      p->namelen = nva[i].namelen;
      data[p->namelen] = '\0';
      nghttp3_downcase(p->name, p->namelen);
      data += nva[i].namelen + 1;
    }

    if (nva[i].flags & NGHTTP3_NV_FLAG_NO_COPY_VALUE) {
      p->value = nva[i].value;
      p->valuelen = nva[i].valuelen;
    } else {
      if (nva[i].valuelen) {
        memcpy(data, nva[i].value, nva[i].valuelen);
      }
      p->value = data;
      p->valuelen = nva[i].valuelen;
      data[p->valuelen] = '\0';
      data += nva[i].valuelen + 1;
    }

    ++p;
  }
  return 0;
}

void nghttp3_nva_del(nghttp3_nv *nva, const nghttp3_mem *mem) {
  nghttp3_mem_free(mem, nva);
}

void nghttp3_frame_headers_free(nghttp3_frame_headers *fr,
                                const nghttp3_mem *mem) {
  if (fr == NULL) {
    return;
  }

  nghttp3_nva_del(fr->nva, mem);
}

void nghttp3_frame_push_promise_free(nghttp3_frame_push_promise *fr,
                                     const nghttp3_mem *mem) {
  if (fr == NULL) {
    return;
  }

  nghttp3_nva_del(fr->nva, mem);
}

nghttp3_pri_elem_type nghttp3_frame_pri_elem_type(uint8_t c) { return c >> 6; }

nghttp3_elem_dep_type nghttp3_frame_elem_dep_type(uint8_t c) {
  return (c >> 4) & 0x3;
}

uint8_t nghttp3_frame_pri_exclusive(uint8_t c) { return (c & 0x8) != 0; }
