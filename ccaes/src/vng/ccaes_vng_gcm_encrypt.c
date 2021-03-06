/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 * 
 * corecrypto Internal Use License Agreement
 * 
 * IMPORTANT:  This Apple corecrypto software is supplied to you by Apple Inc. ("Apple")
 * in consideration of your agreement to the following terms, and your download or use
 * of this Apple software constitutes acceptance of these terms.  If you do not agree
 * with these terms, please do not download or use this Apple software.
 * 
 * 1.	As used in this Agreement, the term "Apple Software" collectively means and
 * includes all of the Apple corecrypto materials provided by Apple here, including
 * but not limited to the Apple corecrypto software, frameworks, libraries, documentation
 * and other Apple-created materials. In consideration of your agreement to abide by the
 * following terms, conditioned upon your compliance with these terms and subject to
 * these terms, Apple grants you, for a period of ninety (90) days from the date you
 * download the Apple Software, a limited, non-exclusive, non-sublicensable license
 * under Apple’s copyrights in the Apple Software to make a reasonable number of copies
 * of, compile, and run the Apple Software internally within your organization only on
 * devices and computers you own or control, for the sole purpose of verifying the
 * security characteristics and correct functioning of the Apple Software; provided
 * that you must retain this notice and the following text and disclaimers in all
 * copies of the Apple Software that you make. You may not, directly or indirectly,
 * redistribute the Apple Software or any portions thereof. The Apple Software is only
 * licensed and intended for use as expressly stated above and may not be used for other
 * purposes or in other contexts without Apple's prior written permission.  Except as
 * expressly stated in this notice, no other rights or licenses, express or implied, are
 * granted by Apple herein.
 * 
 * 2.	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES
 * OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING
 * THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS,
 * SYSTEMS, OR SERVICES. APPLE DOES NOT WARRANT THAT THE APPLE SOFTWARE WILL MEET YOUR
 * REQUIREMENTS, THAT THE OPERATION OF THE APPLE SOFTWARE WILL BE UNINTERRUPTED OR
 * ERROR-FREE, THAT DEFECTS IN THE APPLE SOFTWARE WILL BE CORRECTED, OR THAT THE APPLE
 * SOFTWARE WILL BE COMPATIBLE WITH FUTURE APPLE PRODUCTS, SOFTWARE OR SERVICES. NO ORAL
 * OR WRITTEN INFORMATION OR ADVICE GIVEN BY APPLE OR AN APPLE AUTHORIZED REPRESENTATIVE
 * WILL CREATE A WARRANTY. 
 * 
 * 3.	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING
 * IN ANY WAY OUT OF THE USE, REPRODUCTION, COMPILATION OR OPERATION OF THE APPLE
 * SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
 * NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * 4.	This Agreement is effective until terminated. Your rights under this Agreement will
 * terminate automatically without notice from Apple if you fail to comply with any term(s)
 * of this Agreement.  Upon termination, you agree to cease all use of the Apple Software
 * and destroy all copies, full or partial, of the Apple Software. This Agreement will be
 * governed and construed in accordance with the laws of the State of California, without
 * regard to its choice of law rules.
 * 
 * You may report security issues about Apple products to product-security@apple.com,
 * as described here:  https://www.apple.com/support/security/.  Non-security bugs and
 * enhancement requests can be made via https://bugreport.apple.com as described
 * here: https://developer.apple.com/bug-reporting/
 *
 * EA1350 
 * 10/5/15
 */


#include <corecrypto/cc_runtime_config.h>
#include "ccaes_vng_gcm.h"

#if !defined(__NO_ASM__) && CCMODE_GCM_VNG_SPEEDUP

void ccaes_vng_gcm_encrypt(ccgcm_ctx *key, size_t nbytes,
                       const void *in, void *out) {
    size_t x, y;
    unsigned char b;
    
    if (_CCMODE_GCM_KEY(key)->mode == CCMODE_GCM_MODE_IV) {
        // This allows the gmac routine to be skipped by callers.
        ccaes_vng_gcm_gmac(key, 0, NULL);
    }
    /* in AAD mode? */
    if (_CCMODE_GCM_KEY(key)->mode == CCMODE_GCM_MODE_AAD) {
        /* let's process the AAD */
        if (CCMODE_GCM_KEY_PAD_LEN(key)) {
            _CCMODE_GCM_KEY(key)->totlen += CCMODE_GCM_KEY_PAD_LEN(key) * (uint64_t)(8);
            ccaes_vng_gcm_mult_h(key, CCMODE_GCM_KEY_X(key));
        }

        /* increment counter */
        for (y = 15; y >= 12; y--) {
            if (++CCMODE_GCM_KEY_Y(key)[y] & 255) { break; }
        }
        /* encrypt the counter */
        CCMODE_GCM_KEY_ECB(key)->ecb(CCMODE_GCM_KEY_ECB_KEY(key), 1,
                                     CCMODE_GCM_KEY_Y(key),
                                     CCMODE_GCM_KEY_PAD(key));
        CCMODE_GCM_KEY_PAD_LEN(key) = 0;
        _CCMODE_GCM_KEY(key)->mode   = CCMODE_GCM_MODE_TEXT;
        _CCMODE_GCM_KEY(key)->pttotlen = 0;
    }

    cc_require(_CCMODE_GCM_KEY(key)->mode == CCMODE_GCM_MODE_TEXT,errOut); /* CRYPT_INVALID_ARG */

    x = 0;
    const unsigned char *pt = in;
    unsigned char *ct = out;
#ifdef CCMODE_GCM_FAST
    if (CCMODE_GCM_KEY_PAD_LEN(key) == 0) {

#ifdef  __x86_64__
        if (CC_HAS_AESNI() && CC_HAS_SupplementalSSE3()) 
#endif

        if (nbytes >= 16) {
            unsigned int j = (unsigned int) (nbytes & (-16));
#ifdef  __x86_64__
            if (CC_HAS_AVX1())
                gcmEncrypt_avx1(pt, ct, _CCMODE_GCM_KEY(key), j, CCMODE_GCM_KEY_Htable(key), CCMODE_GCM_KEY_ECB_KEY(key));
            else
                gcmEncrypt_SupplementalSSE3(pt, ct, _CCMODE_GCM_KEY(key), j, CCMODE_GCM_KEY_Htable(key), CCMODE_GCM_KEY_ECB_KEY(key));
#else
            gcmEncrypt(pt, ct, _CCMODE_GCM_KEY(key), j, CCMODE_GCM_KEY_Htable(key), CCMODE_GCM_KEY_ECB_KEY(key));
#endif
            ct += j;    pt += j;    nbytes -= j;
			_CCMODE_GCM_KEY(key)->pttotlen += (j<<3);
#if !defined(__ARM_NEON__) || defined(__arm64__)            
            CCMODE_GCM_KEY_ECB(key)->ecb(CCMODE_GCM_KEY_ECB_KEY(key), 1,
                                         CCMODE_GCM_KEY_Y(key),
                                         CCMODE_GCM_KEY_PAD(key));
#endif

        }

        for (x = 0; x < (nbytes & ~15U); x += 16) {
            /* ctr encrypt */
            for (y = 0; y < 16; y += sizeof(CCMODE_GCM_FAST_TYPE)) {
                *((CCMODE_GCM_FAST_TYPE*)(&ct[x + y])) = *((const CCMODE_GCM_FAST_TYPE*)(&pt[x+y])) ^ *((CCMODE_GCM_FAST_TYPE*)(&CCMODE_GCM_KEY_PAD(key)[y]));
                *((CCMODE_GCM_FAST_TYPE*)(&_CCMODE_GCM_KEY(key)->X[y])) ^= *((CCMODE_GCM_FAST_TYPE*)(&ct[x+y]));
            }
            /* GMAC it */
            _CCMODE_GCM_KEY(key)->pttotlen += 128;
            ccaes_vng_gcm_mult_h(key, _CCMODE_GCM_KEY(key)->X);
            /* increment counter */
            for (y = 15; y >= 12; y--) {
                if (++CCMODE_GCM_KEY_Y(key)[y] & 255) { break; }
            }
            CCMODE_GCM_KEY_ECB(key)->ecb(CCMODE_GCM_KEY_ECB_KEY(key), 1,
                                         CCMODE_GCM_KEY_Y(key),
                                         CCMODE_GCM_KEY_PAD(key));
        }
    }
#endif // CCMODE_GCM_FAST

    /* process text */
    for (; x < nbytes; x++) {
        if (CCMODE_GCM_KEY_PAD_LEN(key) == 16) {
            _CCMODE_GCM_KEY(key)->pttotlen += 128;
            ccaes_vng_gcm_mult_h(key, CCMODE_GCM_KEY_X(key));

            /* increment counter */
            for (y = 15; y >= 12; y--) {
                if (++CCMODE_GCM_KEY_Y(key)[y] & 255) { break; }
            }
            CCMODE_GCM_KEY_ECB(key)->ecb(CCMODE_GCM_KEY_ECB_KEY(key), 1,
                                         CCMODE_GCM_KEY_Y(key),
                                         CCMODE_GCM_KEY_PAD(key));
            CCMODE_GCM_KEY_PAD_LEN(key) = 0;
        }

        b = ct[x] = pt[x] ^ CCMODE_GCM_KEY_PAD(key)[CCMODE_GCM_KEY_PAD_LEN(key)];
        CCMODE_GCM_KEY_X(key)[CCMODE_GCM_KEY_PAD_LEN(key)++] ^= b;
    }
errOut:
    return;
}

#endif

