/* Copyright (C) 2019-2022 Artifex Software, Inc.
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

#ifndef PDF_CHECK
#define PDF_CHECK

int pdfi_check_page(pdf_context *ctx, pdf_dict *page_dict, pdf_array **fonts_array, pdf_array **spots_array, bool do_setup);

int pdfi_check_Pattern_transparency(pdf_context *ctx, pdf_dict *pattern,
                                    pdf_dict *page_dict, bool *transparent);

#endif
