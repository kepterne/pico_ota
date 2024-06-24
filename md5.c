#include	<stdio.h>
#include	<string.h>
#include	<stdint.h>


typedef struct {
	uint32_t hash[4];
	uint32_t bits[2];
	uint8_t data[64];
} MD5Context;


/* Basic MD5 functions */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) (y ^ (z & (x ^ y)))
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define TRANSFORM(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
**
** md5_transform
**
** The MD5 "basic transformation". Updates the hash
** based on the data block passed.
**
*/

static void md5_transform(uint32_t hash[4], const uint32_t data[16]) {
	uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3];

	 /* Round 1 */
	TRANSFORM( F1, a, b, c, d, data[ 0] + 0xd76aa478,  7);
	TRANSFORM( F1, d, a, b, c, data[ 1] + 0xe8c7b756, 12);
	TRANSFORM( F1, c, d, a, b, data[ 2] + 0x242070db, 17);
	TRANSFORM( F1, b, c, d, a, data[ 3] + 0xc1bdceee, 22);
	TRANSFORM( F1, a, b, c, d, data[ 4] + 0xf57c0faf,  7);
	TRANSFORM( F1, d, a, b, c, data[ 5] + 0x4787c62a, 12);
	TRANSFORM( F1, c, d, a, b, data[ 6] + 0xa8304613, 17);
	TRANSFORM( F1, b, c, d, a, data[ 7] + 0xfd469501, 22);
	TRANSFORM( F1, a, b, c, d, data[ 8] + 0x698098d8,  7);
	TRANSFORM( F1, d, a, b, c, data[ 9] + 0x8b44f7af, 12);
	TRANSFORM( F1, c, d, a, b, data[10] + 0xffff5bb1, 17);
	TRANSFORM( F1, b, c, d, a, data[11] + 0x895cd7be, 22);
	TRANSFORM( F1, a, b, c, d, data[12] + 0x6b901122,  7);
	TRANSFORM( F1, d, a, b, c, data[13] + 0xfd987193, 12);
	TRANSFORM( F1, c, d, a, b, data[14] + 0xa679438e, 17);
	TRANSFORM( F1, b, c, d, a, data[15] + 0x49b40821, 22);


	 /* Round 2 */
	TRANSFORM( F2, a, b, c, d, data[ 1] + 0xf61e2562,  5);
	TRANSFORM( F2, d, a, b, c, data[ 6] + 0xc040b340,  9);
	TRANSFORM( F2, c, d, a, b, data[11] + 0x265e5a51, 14);
	TRANSFORM( F2, b, c, d, a, data[ 0] + 0xe9b6c7aa, 20);
	TRANSFORM( F2, a, b, c, d, data[ 5] + 0xd62f105d,  5);
	TRANSFORM( F2, d, a, b, c, data[10] + 0x02441453,  9);
	TRANSFORM( F2, c, d, a, b, data[15] + 0xd8a1e681, 14);
	TRANSFORM( F2, b, c, d, a, data[ 4] + 0xe7d3fbc8, 20);
	TRANSFORM( F2, a, b, c, d, data[ 9] + 0x21e1cde6,  5);
	TRANSFORM( F2, d, a, b, c, data[14] + 0xc33707d6,  9);
	TRANSFORM( F2, c, d, a, b, data[ 3] + 0xf4d50d87, 14);
	TRANSFORM( F2, b, c, d, a, data[ 8] + 0x455a14ed, 20);
	TRANSFORM( F2, a, b, c, d, data[13] + 0xa9e3e905,  5);
	TRANSFORM( F2, d, a, b, c, data[ 2] + 0xfcefa3f8,  9);
	TRANSFORM( F2, c, d, a, b, data[ 7] + 0x676f02d9, 14);
	TRANSFORM( F2, b, c, d, a, data[12] + 0x8d2a4c8a, 20);


	 /* Round 3 */
	TRANSFORM( F3, a, b, c, d, data[ 5] + 0xfffa3942,  4);
	TRANSFORM( F3, d, a, b, c, data[ 8] + 0x8771f681, 11);
	TRANSFORM( F3, c, d, a, b, data[11] + 0x6d9d6122, 16);
	TRANSFORM( F3, b, c, d, a, data[14] + 0xfde5380c, 23);
	TRANSFORM( F3, a, b, c, d, data[ 1] + 0xa4beea44,  4);
	TRANSFORM( F3, d, a, b, c, data[ 4] + 0x4bdecfa9, 11);
	TRANSFORM( F3, c, d, a, b, data[ 7] + 0xf6bb4b60, 16);
	TRANSFORM( F3, b, c, d, a, data[10] + 0xbebfbc70, 23);
	TRANSFORM( F3, a, b, c, d, data[13] + 0x289b7ec6,  4);
	TRANSFORM( F3, d, a, b, c, data[ 0] + 0xeaa127fa, 11);
	TRANSFORM( F3, c, d, a, b, data[ 3] + 0xd4ef3085, 16);
	TRANSFORM( F3, b, c, d, a, data[ 6] + 0x04881d05, 23);
	TRANSFORM( F3, a, b, c, d, data[ 9] + 0xd9d4d039,  4);
	TRANSFORM( F3, d, a, b, c, data[12] + 0xe6db99e5, 11);
	TRANSFORM( F3, c, d, a, b, data[15] + 0x1fa27cf8, 16);
	TRANSFORM( F3, b, c, d, a, data[ 2] + 0xc4ac5665, 23);


	 /* Round 4 */
	TRANSFORM( F4, a, b, c, d, data[ 0] + 0xf4292244,  6);
	TRANSFORM( F4, d, a, b, c, data[ 7] + 0x432aff97, 10);
	TRANSFORM( F4, c, d, a, b, data[14] + 0xab9423a7, 15);
	TRANSFORM( F4, b, c, d, a, data[ 5] + 0xfc93a039, 21);
	TRANSFORM( F4, a, b, c, d, data[12] + 0x655b59c3,  6);
	TRANSFORM( F4, d, a, b, c, data[ 3] + 0x8f0ccc92, 10);
	TRANSFORM( F4, c, d, a, b, data[10] + 0xffeff47d, 15);
	TRANSFORM( F4, b, c, d, a, data[ 1] + 0x85845dd1, 21);
	TRANSFORM( F4, a, b, c, d, data[ 8] + 0x6fa87e4f,  6);
	TRANSFORM( F4, d, a, b, c, data[15] + 0xfe2ce6e0, 10);
	TRANSFORM( F4, c, d, a, b, data[ 6] + 0xa3014314, 15);
	TRANSFORM( F4, b, c, d, a, data[13] + 0x4e0811a1, 21);
	TRANSFORM( F4, a, b, c, d, data[ 4] + 0xf7537e82,  6);
	TRANSFORM( F4, d, a, b, c, data[11] + 0xbd3af235, 10);
	TRANSFORM( F4, c, d, a, b, data[ 2] + 0x2ad7d2bb, 15);
	TRANSFORM( F4, b, c, d, a, data[ 9] + 0xeb86d391, 21);


	hash[ 0 ] += a;
	hash[ 1 ] += b;
	hash[ 2 ] += c;
	hash[ 3 ] += d;
}


/*
** md5_init
**
** Initialise md5 context structure
**
*/
void md5_init(MD5Context *ctx) {
	ctx->hash[ 0 ] = 0x67452301;
	ctx->hash[ 1 ] = 0xefcdab89;
	ctx->hash[ 2 ] = 0x98badcfe;
	ctx->hash[ 3 ] = 0x10325476;

	ctx->bits[ 0 ] = 0;
	ctx->bits[ 1 ] = 0;
}


/*
** md5_update
**
** Update context with the next buffer from the stream of data.
** Call with each block of data to update the md5 hash.
**
*/
void md5_update( MD5Context *ctx, const uint8_t *buf, uint32_t buflen) {
    uint32_t idx;


     /* Update bitcount */


     idx = ctx->bits[ 0 ];
    ctx->bits[ 0 ] = idx + (buflen << 3);
    if( ctx->bits[ 0 ] < idx ) {
        ctx->bits[ 1 ]++;         /* Carry from low to high */
    }
    ctx->bits[ 1 ] += buflen >> 29;


     idx = (idx >> 3) & 0x3f;    /* Bytes already in ctx->data */


     /* Handle any leading odd-sized chunks */


     if( idx != 0 ) {
        uint8_t *p = (uint8_t *) ctx->data + idx;


         idx = 64 - idx;
        if( buflen < idx ) {
            memcpy( p, buf, (size_t) buflen );
        }
        else {
            memcpy( p, buf, (size_t) idx );
            md5_transform( ctx->hash, (uint32_t *) ctx->data );
            buf += idx;
            buflen -= idx;
        }
    }
    if( buflen >= idx ) {
        while( buflen >= 64 ) {
            memcpy( ctx->data, buf, 64 );
            md5_transform( ctx->hash, (uint32_t *) ctx->data );
            buf += 64;
            buflen -= 64;
        }
        memcpy( ctx->data, buf, (size_t) buflen );
    }
}


/*
** md5_final
**
** Finalize creation of md5 hash and copy to digest buffer.
**
*/
void md5_final( MD5Context *ctx, uint8_t digest[ 16 ] )
{
    uint32_t count;
    uint8_t *pad;


     count = (ctx->bits[ 0 ] >> 3) & 0x3F; /* Number of bytes mod 64 */
    pad = ctx->data + count;
    *pad++ = 0x80;


     /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;


     /* Pad out to 56 mod 64 */
    if( count < 8 ) {
        /* Two lots of padding:  Pad the first block to 64 bytes */
        memset( pad, 0, (size_t) count );
        md5_transform( ctx->hash, (uint32_t *) ctx->data );


         /* Now fill the next block with 56 bytes */
        memset( ctx->data, 0, 56 );
    } else {
        /* Pad block to 56 bytes */
        memset( pad, 0, (size_t) (count - 8) );
    }


     /* Append length in bits and transform */
    ((uint32_t *) ctx->data)[ 14 ] = ctx->bits[ 0 ];
    ((uint32_t *) ctx->data)[ 15 ] = ctx->bits[ 1 ];


    md5_transform( ctx->hash, (uint32_t *) ctx->data );
    memcpy( digest, ctx->hash, 16 );
}


/*
** md5_digest_string
**
** Supply the digest and a buffer for the string.
** This routine will populate the buffer and
** return the value as a C string.
**
*/
char *md5_digest_string(uint8_t d[16], char digest_string[33]) {
    int i;


     digest_string[ 32 ] = 0;
    for( i = 0; i < 16; i++ ) {
        sprintf( &(digest_string[ i * 2 ]), "%2.2x", d[ i ] );
    }


     return( digest_string );
}




/*
**
** md5_buffer
**
** Compute the md5 hash on a buffer.
**
*/
void md5_buffer( const uint8_t *buf, int buflen, uint8_t digest[16]) {
    MD5Context ctx;

    md5_init(&ctx);
    md5_update(&ctx, buf, buflen);
    md5_final(&ctx, digest);
}
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
void encodeblock( unsigned char in[3], unsigned char out[4], int len) {
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

void	md5_buffer_b64(const uint8_t *buf, int buflen, char *str) {
	uint8_t		digest[16];
	MD5Context	ctx;
	int			i = 0; //, j = 0;

    md5_init(&ctx);
    md5_update(&ctx, buf, buflen);
    md5_final(&ctx, digest);
	for (i=0; i<15; i+=3, str+=4) {
		encodeblock(digest+i, (unsigned char *) str, 3);
	}
	encodeblock(digest+i, (unsigned char *) str, 1);
	str++;
	str++;
	*str = 0;
}