#ifndef _ASN1_H
#define _ASN1_H

enum Tag_Class { TC_UNIVERSAL, TC_APPLICATION, TC_CONTEXT, TC_PRIVATE };

/*
 * Attributes of ASN.1 tag.
 *
 * These values are encoded in tag's identifier and length octets.
 */
struct ASN1_Header {
	enum Tag_Class cls; /* Tag class. */
	unsigned int num; /* Tag number. */
	bool cons_p; /* Is encoding constructed? */
	size_t len; /* Length of contents. */
};

#endif /* _ASN1_H */
