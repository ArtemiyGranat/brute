/*
 * Copyright (C) 1991-2018 Free Software Foundation, Inc.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the GNU C Library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "crypt.h"

#include <stdlib.h>
#include <errno.h>

char *
crypt_r (const char *key, const char *salt,
	   struct crypt_data * __restrict data)
{
  ufc_long res[4];
  char ktab[9];
  ufc_long xx = 25; /* to cope with GCC long long compiler bugs */

#ifdef _LIBC
  /* Try to find out whether we have to use MD5 encryption replacement.  */
  if (strncmp (md5_salt_prefix, salt, sizeof (md5_salt_prefix) - 1) == 0)
    {
      return __md5_crypt_r (key, salt, (char *) data,
			    sizeof (struct crypt_data));
    }

  /* Try to find out whether we have to use SHA256 encryption replacement.  */
  if (strncmp (sha256_salt_prefix, salt, sizeof (sha256_salt_prefix) - 1) == 0)
    return __sha256_crypt_r (key, salt, (char *) data,
			     sizeof (struct crypt_data));

  /* Try to find out whether we have to use SHA512 encryption replacement.  */
  if (strncmp (sha512_salt_prefix, salt, sizeof (sha512_salt_prefix) - 1) == 0)
    return __sha512_crypt_r (key, salt, (char *) data,
			     sizeof (struct crypt_data));
#endif

  /*
   * Hack DES tables according to salt
   */
  if (!_ufc_setup_salt_r (salt, data))
    {
      __set_errno (EINVAL);
      return NULL;
    }

  /*
   * Setup key schedule
   */
  _ufc_clearmem (ktab, (int) sizeof (ktab));
  (void) strncpy (ktab, key, 8);
  _ufc_mk_keytab_r (ktab, data);

  /*
   * Go for the 25 DES encryptions
   */
  _ufc_clearmem ((char*) res, (int) sizeof (res));
  _ufc_doit_r (xx,  data, &res[0]);

  /*
   * Do final permutations
   */
  _ufc_dofinalperm_r (res, data);

  /*
   * And convert back to 6 bit ASCII
   */
  _ufc_output_conversion_r (res[0], res[1], salt, data);

  /*
   * Erase key-dependent intermediate data.  Data dependent only on
   * the salt is not considered sensitive.
   */
  explicit_bzero (ktab, sizeof (ktab));
  explicit_bzero (data->keysched, sizeof (data->keysched));
  explicit_bzero (res, sizeof (res));

  return data->crypt_3_buf;
}