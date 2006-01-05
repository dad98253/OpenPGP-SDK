#include <openpgpsdk/packet.h>
#include <openpgpsdk/packet-parse.h>
#include <openpgpsdk/packet-show.h>
#include <openpgpsdk/configure.h>
#include <openpgpsdk/util.h>
#include <openpgpsdk/errors.h>
#include <openpgpsdk/armour.h>
#include <openpgpsdk/crypto.h>

#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int indent=0;
static const char *pname;

static void print_indent()
    {
    int i=0;

    for(i=0 ; i < indent ; i++)
	printf("  ");
    }

static void showtime(const char *name,time_t t)
    {
    printf("%s=" TIME_T_FMT " (%.24s)",name,t,ctime(&t));
    }

static void print_bn( const char *name, const BIGNUM *bn)
    {
    print_indent();
    printf("%s=",name);
    BN_print_fp(stdout,bn);
    printf("\n");
    }

static void print_time( char *name, time_t time)
    {
    print_indent();
    printf("%s: ",name);
    showtime("time",time);
    printf("\n");
    }

static void print_duration(char *name, time_t time)
    {
    int mins, hours, days, years;

    print_indent();
    printf("%s: ",name);
    printf("duration " TIME_T_FMT " seconds",time);

    mins=time/60;
    hours=mins/60;
    days=hours/24;
    years=days/365;

    printf(" (approx. ");
    if (years)
	printf("%d %s",years,years==1?"year":"years");
    else if (days)
	printf("%d %s",days,days==1?"day":"days");
    else if (hours)
	printf("%d %s", hours, hours==1?"hour":"hours");

    printf(")");
    printf("\n");
    }

static void print_name(const char *name)
    {
    print_indent();
    if(name)
	printf("%s: ",name);
    }

static void print_text_breakdown( ops_text_t *text)
    {
    unsigned i;
    char *prefix=".. ";

    /* these were recognised */

    for(i=0 ; i<text->known.used ; i++)
	{
	print_indent();
	printf(prefix);
	printf("%s\n",text->known.strings[i]);
	}

    /* these were not recognised. the strings will contain the hex value
       of the unrecognised value in string format - see process_octet_str()
    */

    if(text->unknown.used)
	{
	printf("\n");
	print_indent();
	printf("Not Recognised: ");
	}
    for( i=0; i < text->unknown.used; i++) 
	{
	print_indent();
	printf(prefix);
	printf("%s\n",text->unknown.strings[i]);
	}
	
    }

static void printhex(const unsigned char *src,size_t length)
    {
    while(length--)
	printf("%02X",*src++);
    }

static void print_hexdump(const char *name,
			  const unsigned char *data,
			  unsigned int len)
    {
    print_name(name);

    printf("len=%d, data=0x", len);
    printhex(data,len);
    printf("\n");
    }

static void print_hexdump_data(const char *name,
			       const unsigned char *data,
			       unsigned int len)
    {
    print_name(name);

    printf("0x");
    printhex(data,len);
    printf("\n");
    }

static void print_data(const char *name,const ops_data_t *data)
    {
    print_hexdump(name,data->contents,data->len);
    }


static void print_boolean(const char *name, unsigned char bool)
    {
    print_name(name);

    if(bool)
	printf("Yes");
    else
	printf("No");
    printf("\n");
    }

static void print_tagname(const char *str)
    {
    print_indent();
    printf("%s packet\n", str);
    }

static void print_string(const char *name,const char *str)
    {
    print_name(name);
    while(*str)
	{
	if(*str >= 0x20 && *str < 0x7f && *str != '%')
	    putchar(*str);
	else
	    printf("%%%02x",(unsigned char)*str);
	++str;
	}
    putchar('\n');
    }

static void print_utf8_string(const char *name,const unsigned char *str)
    {
    // \todo Do this better for non-English character sets
    print_string(name,(const char *)str);
    }

static void print_block(const char *name,const unsigned char *str,
			size_t length)
    {
    int o=length;

    print_indent();
    printf(">>>>> %s >>>>>\n",name);

    print_indent();
    for( ; length > 0 ; --length)
	{
	if(*str >= 0x20 && *str < 0x7f && *str != '%')
	    putchar(*str);
	else if(*str == '\n')
	    {
	    putchar(*str);
	    print_indent();
	    }
	else
	    printf("%%%02x",*str);
	++str;
	}
    if(o && str[-1] != '\n')
	{
	putchar('\n');
	print_indent();
	fputs("[no newline]",stdout);
	}
    else
	print_indent();
    printf("<<<<< %s <<<<<\n",name);
    }

static void print_headers(const ops_headers_t *headers)
    {
    unsigned n;

    for(n=0 ; n < headers->nheaders ; ++n)
	printf("%s=%s\n",headers->headers[n].key,headers->headers[n].value);
    }

static void print_unsigned_int(char *name, unsigned int val)
    {
    print_name(name);
    printf("%d\n", val);
    }

static void print_string_and_value( char *name, char *str, unsigned char value)
    {
    print_name(name);

    printf("%s", str);
    printf(" (0x%x)", value);
    printf("\n");
    }

static void start_subpacket(unsigned type)
    {
    indent++;
    print_indent();
    printf("-- %s (type 0x%02x)\n",
	   ops_show_ss_type(type),
	   type-OPS_PTAG_SIGNATURE_SUBPACKET_BASE);
    }
 
static void end_subpacket()
    {
    indent--;
    }

static void print_packet(const ops_packet_t *packet)
    {
    unsigned char *cur;
    int i;
    int rem;
    int blksz=4;

    printf("\nhexdump of packet contents follows:\n");


    for (i=1,cur=packet->raw; cur<(packet->raw+packet->length); cur+=blksz,i++)
	{
	rem = packet->raw+packet->length-cur;
	hexdump(cur,rem<=blksz ? rem : blksz);
	printf(" ");
	if (!(i%8))
	    printf("\n");
	
	}
    
    printf("\n");
    }

static void print_public_key(const ops_public_key_t *key)
    {
    print_unsigned_int("Version",key->version);
    print_time("Creation Time", key->creation_time);
    if(key->version == OPS_V3)
	print_unsigned_int("Days Valid",key->days_valid);

    print_string_and_value("Algorithm",ops_show_pka(key->algorithm),
			   key->algorithm);

    switch(key->algorithm)
	{
    case OPS_PKA_DSA:
	print_bn("p",key->key.dsa.p);
	print_bn("q",key->key.dsa.q);
	print_bn("g",key->key.dsa.g);
	print_bn("y",key->key.dsa.y);
	break;

    case OPS_PKA_RSA:
    case OPS_PKA_RSA_ENCRYPT_ONLY:
    case OPS_PKA_RSA_SIGN_ONLY:
	print_bn("n",key->key.rsa.n);
	print_bn("e",key->key.rsa.e);
	break;

    case OPS_PKA_ELGAMAL:
    case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
	print_bn("p",key->key.elgamal.p);
	print_bn("g",key->key.elgamal.g);
	print_bn("y",key->key.elgamal.y);
	break;

    default:
	assert(0);
	}
    }

static ops_parse_cb_return_t callback(const ops_parser_content_t *content_,
				      ops_parse_cb_info_t *cbinfo)
    {
    const ops_parser_content_union_t *content=&content_->content;
    ops_text_t *text;
    char *str;

    OPS_USED(cbinfo);

    switch(content_->tag)
	{
    case OPS_PARSER_ERROR:
	printf("parse error: %s\n",content->error.error);
	break;

    case OPS_PARSER_ERRCODE:
	printf("parse error: %s\n",
	       ops_errcode(content->errcode.errcode));
	break;

    case OPS_PARSER_PACKET_END:
	print_packet(&content->packet);
	break;

    case OPS_PARSER_PTAG:

	if (content->ptag.content_tag==OPS_PTAG_CT_PUBLIC_KEY)
	    {
	    indent=0;
	    printf("\n*** NEXT KEY ***\n");
	    }

	printf("\n");
	print_indent();
	printf("==== ptag new_format=%d content_tag=%d length_type=%d"
	       " length=0x%x (%d) position=0x%x (%d)\n",content->ptag.new_format,
	       content->ptag.content_tag,content->ptag.length_type,
	       content->ptag.length,content->ptag.length,
	       content->ptag.position,content->ptag.position);
	/*
	print_tagname(ops_str_from_single_packet_tag(content->ptag.content_tag));
	*/
	break;

    case OPS_PTAG_CT_PUBLIC_KEY:
    case OPS_PTAG_CT_PUBLIC_SUBKEY:
	if (content_->tag == OPS_PTAG_CT_PUBLIC_KEY)
	    print_tagname("PUBLIC KEY");
	else
	    print_tagname("PUBLIC SUBKEY");

	print_public_key(&content->public_key);
	break;

    case OPS_PTAG_CT_TRUST:
	print_tagname("TRUST");
	print_data("Trust",&content->trust.data);
	break;
	
    case OPS_PTAG_CT_USER_ID:
	/* XXX: how do we print UTF-8? */
	print_tagname("USER ID");
	print_utf8_string("user_id",content->user_id.user_id);
	break;

    case OPS_PTAG_CT_SIGNATURE:
	print_tagname("SIGNATURE");
	print_indent(indent);
	print_unsigned_int("Signature Version",
	       content->signature.version);
	if (content->signature.version == 3) 
	    print_time("Signature Creation Time", content->signature.creation_time);

	print_string_and_value("Signature Type",
			       ops_show_sig_type(content->signature.type),
			       content->signature.type);

	print_hexdump_data("Signer ID",
		      content->signature.signer_id,
		      sizeof content->signature.signer_id);

	print_string_and_value("Public Key Algorithm",
			       ops_show_pka(content->signature.key_algorithm),
			       content->signature.key_algorithm);
	print_string_and_value("Hash Algorithm",
			       ops_show_hash_algorithm(content->signature.hash_algorithm),
			       content->signature.hash_algorithm);

	print_indent();
	print_hexdump_data("hash2",&content->signature.hash2[0],2);

	switch(content->signature.key_algorithm)
	    {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_SIGN_ONLY:
	    print_bn("sig",content->signature.signature.rsa.sig);
	    break;

	case OPS_PKA_DSA:
	    print_bn("r",content->signature.signature.dsa.r);
	    print_bn("s",content->signature.signature.dsa.s);
	    break;

	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
	    print_bn("r",content->signature.signature.elgamal.r);
	    print_bn("s",content->signature.signature.elgamal.s);
	    break;

	default:
	    assert(0);
	    }    
	break;

    case OPS_PTAG_CT_COMPRESSED:
	print_tagname("COMPRESSED");
	print_unsigned_int("Compressed Data Type", content->compressed.type);
	break;

    case OPS_PTAG_CT_ONE_PASS_SIGNATURE:
	print_tagname("ONE PASS SIGNATURE");

	print_unsigned_int("Version",content->one_pass_signature.version);
	print_string_and_value("Signature Type",
			       ops_show_sig_type(content->one_pass_signature.sig_type),
			       content->one_pass_signature.sig_type);
	print_string_and_value("Hash Algorithm",
			       ops_show_hash_algorithm(content->one_pass_signature.hash_algorithm),
			       content->one_pass_signature.hash_algorithm);
	print_string_and_value("Public Key Algorithm",
			       ops_show_pka(content->one_pass_signature.key_algorithm),
			       content->one_pass_signature.key_algorithm);
 	print_hexdump("Signer ID",
		      content->one_pass_signature.keyid,
		      sizeof content->one_pass_signature.keyid);

	print_unsigned_int("Nested",
			   content->one_pass_signature.nested);
	break;

    case OPS_PTAG_CT_USER_ATTRIBUTE:
	print_tagname("USER ATTRIBUTE");
	print_hexdump("User Attribute",
		      content->user_attribute.data.contents,
		      content->user_attribute.data.len);
	break;

    case OPS_PTAG_RAW_SS:
	assert(!content_->critical);
	start_subpacket(content_->tag);
	print_unsigned_int("Raw Signature Subpacket: tag",
			   content->ss_raw.tag-OPS_PTAG_SIGNATURE_SUBPACKET_BASE);
	print_hexdump("Raw Data",
		      content->ss_raw.raw,
		      content->ss_raw.length);
	break;

    case OPS_PTAG_SS_CREATION_TIME:
	start_subpacket(content_->tag);
	print_time("Signature Creation Time",content->ss_time.time);
	end_subpacket();
	break;

    case OPS_PTAG_SS_EXPIRATION_TIME:
	start_subpacket(content_->tag);
	print_duration("Signature Expiration Time",content->ss_time.time);
	end_subpacket();
	break;

    case OPS_PTAG_SS_KEY_EXPIRATION_TIME:
	start_subpacket(content_->tag);
	print_duration("Key Expiration Time", content->ss_time.time);
	end_subpacket();
	break;

    case OPS_PTAG_SS_TRUST:
	start_subpacket(content_->tag);
	print_string("Trust Signature","");
	print_unsigned_int("Level",
			   content->ss_trust.level);
	print_unsigned_int("Amount",
			   content->ss_trust.amount);
	end_subpacket();
	break;
		
    case OPS_PTAG_SS_REVOCABLE:
	start_subpacket(content_->tag);
	print_boolean("Revocable",content->ss_revocable.revocable);
	end_subpacket();
	break;      

    case OPS_PTAG_SS_REVOCATION_KEY:
	start_subpacket(content_->tag);
	/* not yet tested */
	printf ("  revocation key: class=0x%x",
		content->ss_revocation_key.class);
	if (content->ss_revocation_key.class&0x40)
	    printf (" (sensitive)");
	printf (", algid=0x%x",
		content->ss_revocation_key.algid);
	printf(", fingerprint=");
	hexdump(content->ss_revocation_key.fingerprint,20);
	printf("\n");
	end_subpacket();
	break;
    
    case OPS_PTAG_SS_ISSUER_KEY_ID:
	start_subpacket(content_->tag);
	print_hexdump("Issuer Key Id",
		      &content->ss_issuer_key_id.key_id[0],
		      sizeof content->ss_issuer_key_id.key_id);
	end_subpacket();
	break;

    case OPS_PTAG_SS_PREFERRED_SKA:
	start_subpacket(content_->tag);
	print_data( "Preferred Symmetric Algorithms",
		   &content->ss_preferred_ska.data);

	text = ops_showall_ss_preferred_ska(content->ss_preferred_ska);
	print_text_breakdown(text);
	ops_text_free(text);

	end_subpacket();
   	break;

    case OPS_PTAG_SS_PRIMARY_USER_ID:
	start_subpacket(content_->tag);
	print_boolean("Primary User ID",
		      content->ss_primary_user_id.primary_user_id);
	end_subpacket();
	break;      

    case OPS_PTAG_SS_PREFERRED_HASH:
	start_subpacket(content_->tag);
	print_data( "Preferred Hash Algorithms",
		   &content->ss_preferred_hash.data);

	text = ops_showall_ss_preferred_hash(content->ss_preferred_hash);
	print_text_breakdown(text);
	ops_text_free(text);
	end_subpacket();
	break;

    case OPS_PTAG_SS_PREFERRED_COMPRESSION:
	start_subpacket(content_->tag);
	print_data( "Preferred Compression Algorithms",
		   &content->ss_preferred_compression.data);

	text = ops_showall_ss_preferred_compression(content->ss_preferred_compression);
	print_text_breakdown(text);
	ops_text_free(text);
	end_subpacket();
	break;
	
    case OPS_PTAG_SS_KEY_FLAGS:
	start_subpacket(content_->tag);
	print_data( "Key Flags", &content->ss_key_flags.data);

	text = ops_showall_ss_key_flags(content->ss_key_flags);
	print_text_breakdown( text);
	ops_text_free(text);

	end_subpacket();
	break;
	
    case OPS_PTAG_SS_KEY_SERVER_PREFS:
	start_subpacket(content_->tag);
	print_data( "Key Server Preferences",
		   &content->ss_key_server_prefs.data);

	text = ops_showall_ss_key_server_prefs(content->ss_key_server_prefs);
	print_text_breakdown( text);
	ops_text_free(text);

	end_subpacket();
	break;
	
    case OPS_PTAG_SS_FEATURES:
	start_subpacket(content_->tag);
	print_data( "Features", 
		   &content->ss_features.data);

	text = ops_showall_ss_features(content->ss_features);
	print_text_breakdown( text);
	ops_text_free(text);

	end_subpacket();
	break;

    case OPS_PTAG_SS_NOTATION_DATA:
	start_subpacket(content_->tag);
	print_indent();
	printf("Notation Data:\n");

	indent++;
	print_data( "Flags",
		   &content->ss_notation_data.flags);
	text = ops_showall_ss_notation_data_flags(content->ss_notation_data);
	print_text_breakdown( text);
	ops_text_free(text);

	/* xxx - TODO: print out UTF - rachel */

	print_data( "Name",
		   &content->ss_notation_data.name);

	print_data( "Value",
		   &content->ss_notation_data.value);

	indent--;
	end_subpacket();
	break;

    case OPS_PTAG_SS_REGEXP:
	start_subpacket(content_->tag);
	print_hexdump("Regular Expression",
		      (unsigned char *)content->ss_regexp.text,
		      strlen(content->ss_regexp.text));
	print_string(NULL,
		     content->ss_regexp.text);
	end_subpacket();
	break;

    case OPS_PTAG_SS_POLICY_URL:
	start_subpacket(content_->tag);
	print_string("Policy URL",
		     content->ss_policy_url.text);
	end_subpacket();
	break;

    case OPS_PTAG_SS_SIGNERS_USER_ID:
	start_subpacket(content_->tag);
	print_utf8_string("Signer's User ID",content->ss_signers_user_id.user_id);
	end_subpacket();
	break;

    case OPS_PTAG_SS_PREFERRED_KEY_SERVER:
	start_subpacket(content_->tag);
	print_string("Preferred Key Server",
		     content->ss_preferred_key_server.text);
	end_subpacket();
	break;

    case OPS_PTAG_SS_USERDEFINED00:
    case OPS_PTAG_SS_USERDEFINED01:
    case OPS_PTAG_SS_USERDEFINED02:
    case OPS_PTAG_SS_USERDEFINED03:
    case OPS_PTAG_SS_USERDEFINED04:
    case OPS_PTAG_SS_USERDEFINED05:
    case OPS_PTAG_SS_USERDEFINED06:
    case OPS_PTAG_SS_USERDEFINED07:
    case OPS_PTAG_SS_USERDEFINED08:
    case OPS_PTAG_SS_USERDEFINED09:
    case OPS_PTAG_SS_USERDEFINED10:
	start_subpacket(content_->tag);
	print_hexdump("Internal or user-defined",
		      content->ss_userdefined.data.contents,
		      content->ss_userdefined.data.len);
	end_subpacket();
	break;

    case OPS_PTAG_SS_RESERVED:
	start_subpacket(content_->tag);
	print_hexdump("Reserved",
		      content->ss_userdefined.data.contents,
		      content->ss_userdefined.data.len);
	end_subpacket();
	break;

    case OPS_PTAG_SS_REVOCATION_REASON:
	start_subpacket(content_->tag);
	print_hexdump("Revocation Reason",
		      &content->ss_revocation_reason.code,
		      1);
	str = ops_show_ss_rr_code(content->ss_revocation_reason.code);
	print_string(NULL,str);
	/* xxx - todo : output text as UTF-8 string */
	end_subpacket();
	break;

    case OPS_PTAG_CT_LITERAL_DATA_HEADER:
	print_tagname("LITERAL DATA HEADER");
	printf("  literal data header format=%c filename='%s'\n",
	       content->literal_data_header.format,
	       content->literal_data_header.filename);
	showtime("    modification time",
		 content->literal_data_header.modification_time);
	printf("\n");
	break;

    case OPS_PTAG_CT_LITERAL_DATA_BODY:
	print_tagname("LITERAL DATA BODY");
	printf("  literal data body length=%d\n",
	       content->literal_data_body.length);
	printf("    data=");
	hexdump(content->literal_data_body.data,
		content->literal_data_body.length);
	printf("\n");
	break;

    case OPS_PTAG_CT_SIGNATURE_HEADER:
	print_tagname("SIGNATURE");
	print_indent(indent);
	print_unsigned_int("Signature Version",
	       content->signature.version);
	if(content->signature.creation_time_set) 
	    print_time("Signature Creation Time", content->signature.creation_time);

	print_string_and_value("Signature Type",
			       ops_show_sig_type(content->signature.type),
			       content->signature.type);

	if(content->signature.signer_id_set)
	    print_hexdump_data("Signer ID",
			       content->signature.signer_id,
			       sizeof content->signature.signer_id);

	print_string_and_value("Public Key Algorithm",
			       ops_show_pka(content->signature.key_algorithm),
			       content->signature.key_algorithm);
	print_string_and_value("Hash Algorithm",
			       ops_show_hash_algorithm(content->signature.hash_algorithm),
			       content->signature.hash_algorithm);

	break;

    case OPS_PTAG_CT_SIGNATURE_FOOTER:
	print_indent();
	print_hexdump_data("hash2",&content->signature.hash2[0],2);

	switch(content->signature.key_algorithm)
	    {
	case OPS_PKA_RSA:
	    print_bn("sig",content->signature.signature.rsa.sig);
	    break;

	case OPS_PKA_DSA:
	    print_bn("r",content->signature.signature.dsa.r);
	    print_bn("s",content->signature.signature.dsa.s);
	    break;

	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
	    print_bn("r",content->signature.signature.elgamal.r);
	    print_bn("s",content->signature.signature.elgamal.s);
	    break;

	case OPS_PKA_PRIVATE00:
	case OPS_PKA_PRIVATE01:
	case OPS_PKA_PRIVATE02:
	case OPS_PKA_PRIVATE03:
	case OPS_PKA_PRIVATE04:
	case OPS_PKA_PRIVATE05:
	case OPS_PKA_PRIVATE06:
	case OPS_PKA_PRIVATE07:
	case OPS_PKA_PRIVATE08:
	case OPS_PKA_PRIVATE09:
	case OPS_PKA_PRIVATE10:
	    print_data("Private/Experimental",
		       &content->signature.signature.unknown.data);
	    break;

	default:
	    assert(0);
	    }
	break;

    case OPS_PTAG_CT_SECRET_KEY:
	// XXX: fix me
	print_tagname("SECRET_KEY");
	print_public_key(&content->secret_key.public_key);

	switch(content->secret_key.public_key.algorithm)
	    {
	case OPS_PKA_RSA:
	    print_bn("d",content->secret_key.key.rsa.d);
	    print_bn("p",content->secret_key.key.rsa.p);
	    print_bn("q",content->secret_key.key.rsa.q);
	    print_bn("u",content->secret_key.key.rsa.u);
	    break;

	default:
	    assert(0);
	    }
	break;

    case OPS_PTAG_CT_ARMOUR_HEADER:
	print_tagname("ARMOUR HEADER");
	print_string("type",content->armour_header.type);
	break;

    case OPS_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
	print_tagname("SIGNED CLEARTEXT HEADER");
	print_headers(&content->signed_cleartext_header.headers);
	break;

    case OPS_PTAG_CT_SIGNED_CLEARTEXT_BODY:
	print_tagname("SIGNED CLEARTEXT BODY");
	print_block("signed cleartext",content->signed_cleartext_body.data,
		    content->signed_cleartext_body.length);
	break;

    case OPS_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
	print_tagname("SIGNED CLEARTEXT TRAILER");
	printf("hash algorithm: %d\n",
	       content->signed_cleartext_trailer.hash->algorithm);
	printf("\n");
	break;

    case OPS_PTAG_CT_UNARMOURED_TEXT:
	print_tagname("UNARMOURED TEXT");
	print_block("unarmoured text",content->unarmoured_text.data,
		    content->unarmoured_text.length);
	break;

    case OPS_PTAG_CT_ARMOUR_TRAILER:
	print_tagname("ARMOUR TRAILER");
	print_string("type",content->armour_header.type);
	break;

    case OPS_PTAG_CT_PK_SESSION_KEY:
	print_tagname("PUBLIC KEY SESSION KEY");
	printf("Version: %d\n",content->pk_session_key.version);
	print_hexdump("key ID",content->pk_session_key.key_id,
		      sizeof content->pk_session_key.key_id);
	printf("Algorithm: %d\n",content->pk_session_key.algorithm);
	switch(content->pk_session_key.algorithm)
	    {
	case OPS_PKA_RSA:
	    print_bn("encrypted_m",
		     content->pk_session_key.parameters.rsa.encrypted_m);
	    break;

	case OPS_PKA_ELGAMAL:
	    print_bn("g_to_k",
		     content->pk_session_key.parameters.elgamal.g_to_k);
	    print_bn("encrypted_m",
		     content->pk_session_key.parameters.elgamal.encrypted_m);
	    break;

	default:
	    assert(0);
	    }
	break;

    default:
	print_tagname("UNKNOWN PACKET TYPE");
	fprintf(stderr,"packet-dump: unknown tag=%d (0x%x)\n",content_->tag,
		content_->tag);
	exit(1);
	}
    return OPS_RELEASE_MEMORY;
    }

static void usage()
    {
    fprintf(stderr,"%s [-a]\n",pname);
    exit(1);
    }

int main(int argc,char **argv)
    {
    ops_parse_info_t *pinfo;
    ops_boolean_t armour=ops_false;
    int ret;
    int ch;

    pname=argv[0];

    while((ch=getopt(argc,argv,"ab")) != -1)
	switch(ch)
	    {
	case 'a':
	    armour=ops_true;
	    break;

	case 'b':
	    setvbuf(stdout,NULL,_IONBF,0);
	    setvbuf(stderr,NULL,_IONBF,0);
	    break;

	default:
	    usage();
	    }


    pinfo=ops_parse_info_new();
    //    ops_parse_packet_options(&opt,OPS_PTAG_SS_ALL,OPS_PARSE_RAW);
    ops_parse_options(pinfo,OPS_PTAG_SS_ALL,OPS_PARSE_PARSED);

    ops_parse_cb_set(pinfo,callback,NULL);

    ops_reader_set_fd(pinfo,0);

    if(armour)
	ops_reader_push_dearmour(pinfo,ops_true,ops_true,ops_true);

    ret=ops_parse(pinfo);
    if (!ret)
	ops_print_errors(ops_parse_info_get_errors(pinfo));

    ops_parse_info_delete(pinfo);

    return 0;
    }
