#include "parser.h"

using namespace std;

static void rndr_blockcode(struct buf *ob, struct buf *text, void *opaque);
static void rndr_blockquote(struct buf *ob, struct buf *text, void *opaque);
static void rndr_header(struct buf *ob, struct buf *text, int level, void *opaque);
static void rndr_list(struct buf *ob, struct buf *text, int flags, void *opaque);
static void rndr_listitem(struct buf *ob, struct buf *text, int flags, void *opaque);
static void rndr_paragraph(struct buf *ob, struct buf *text, void *opaque);
static int rndr_codespan(struct buf *ob, struct buf *text, void *opaque);
static int rndr_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque);
static int rndr_emphasis(struct buf *ob, struct buf *text, char c, void *opaque);
static int rndr_triple_emphasis(struct buf *ob, struct buf *text, char c, void *opaque);
static int rndr_linebreak(struct buf *ob, void *opaque);
static int rndr_link(struct buf *ob, struct buf *link, struct buf *title, struct buf *content, void *opaque);
static void rndr_normal_text(struct buf *ob, struct buf *text, void *opaque);

struct mkd_renderer mkd_callbacks = {
	/* document-level callbacks */
	NULL,                 // prolog
	NULL,                 // epilogue

	/* block-level callbacks */
	rndr_blockcode,       // block code
	rndr_blockquote,      // block quote
	NULL,                 // block html
	rndr_header,          // header
	NULL,                 // hrule
	rndr_list,            // list
	rndr_listitem,        // listitem
	rndr_paragraph,       // paragraph
	NULL,                 // table
	NULL,                 // table cell
	NULL,                 // table row

	/* span-level callbacks */
	NULL,                 // autolink
	rndr_codespan,        // codespan
	rndr_double_emphasis, // double emphasis
	rndr_emphasis,        // emphasis
	NULL,                 // image
	rndr_linebreak,       // line break
	rndr_link,            // link
	NULL,                 // raw html tag
	rndr_triple_emphasis, // triple emphasis

	/* low-level callbacks */
	NULL,                 // entity
	rndr_normal_text,     // normal text

	/* renderer data */
	64, // max stack
	"*_",
	NULL // opaque
};

namespace Bypass {

	const static std::string TWO_SPACES = "  ";

	Parser::Parser()
	: elementSoup()
	{
		elementCount = 1;
	}

	Parser::~Parser() {

	}

	Document Parser::parse(const char* mkd) {
		document = Document();

		if (mkd) {
			struct buf *ib, *ob;

			ib = bufnew(INPUT_UNIT);
			bufputs(ib, mkd);

			ob = bufnew(OUTPUT_UNIT);

			mkd_callbacks.opaque = this;

			//parse and assemble document
			markdown(ob, ib, &mkd_callbacks);

			for (boost::unordered_map<std::string, Element>::iterator it = elementSoup.begin(); it != elementSoup.end(); ++it) {
				document.append(it->second);
			}

			bufrelease(ib);
			bufrelease(ob);
		}

		return document;
	}

	Document Parser::parse(const string& markdown) {
		return parse(markdown.c_str());
	}

	// Block Element Callbacks

	void Parser::handleBlock(Type type, struct buf *ob, struct buf *text) {
		Element block;
		block.setType(type);

		std::string textString(text->data);
		textString = textString.substr(0,text->size);
		std::vector<std::string> strs;
		boost::split(strs, textString, boost::is_any_of("|"));

		for(vector<std::string>::iterator it = strs.begin(); it != strs.end(); it++) {
			if (elementSoup.count(*it) > 0) {
				block.append(elementSoup.at(*it));
				elementSoup.erase(*it);
			}
		}

		elementCount++;

		std::ostringstream oss;
		oss << elementCount;
		elementSoup[oss.str()] = block;
		oss << '|';
		bufputs(ob, oss.str().c_str());
	}

	void Parser::parsedBlockcode(struct buf *ob, struct buf *text) {
		handleBlock(BLOCK_CODE, ob, text);
	}

	void Parser::parsedBlockquote(struct buf *ob, struct buf *text) {
		handleBlock(BLOCK_QUOTE, ob, text);
	}

	void Parser::parsedHeader(struct buf *ob, struct buf *text, int level) {
		handleBlock(HEADER, ob, text);
	}

	void Parser::parsedList(struct buf *ob, struct buf *text, int flags) {
		handleBlock(LIST, ob, text);
	}

	void Parser::parsedListItem(struct buf *ob, struct buf *text, int flags) {
		handleBlock(LIST_ITEM, ob, text);
	}

	void Parser::parsedParagraph(struct buf *ob, struct buf *text) {
		handleBlock(PARAGRAPH, ob, text);
	}

	// Span Element Callbacks

	void Parser::handleSpan(Type type, struct buf *ob, struct buf *text, struct buf *extra) {


		std::string textString(text->data);
		textString = textString.substr(0, text->size);
		std::vector<std::string> strs;
		if (text)
			boost::split(strs, textString, boost::is_any_of("|"));
		if (strs.size() > 0) {
			Element element = elementSoup.at(strs[0]);
			element.setType(type);
			elementSoup.erase(strs[0]);
			elementSoup[strs[0]] = element;

			bufputs(ob, textString.c_str());
		}
		else {
			Element element;
			element.setType(type);

			createSpan(element, ob);
		}
	}

	void Parser::createSpan(Element element, struct buf *ob) {
		elementCount++;
		std::ostringstream oss;
		oss << elementCount;
		elementSoup[oss.str()] = element;
		oss << '|';
		bufputs(ob, oss.str().c_str());
	}

	int Parser::parsedDoubleEmphasis(struct buf *ob, struct buf *text, char c) {
		handleSpan(DOUBLE_EMPHASIS, ob, text);
		return 1;
	}

	int Parser::parsedEmphasis(struct buf *ob, struct buf *text, char c) {
		handleSpan(EMPHASIS, ob, text);
		return 1;
	}

	int Parser::parsedTripleEmphasis(struct buf *ob, struct buf *text, char c) {
		handleSpan(TRIPLE_EMPHASIS, ob, text);
		return 1;
	}

	int Parser::parsedLink(struct buf *ob, struct buf *link, struct buf *title, struct buf *content) {
		handleSpan(LINK, ob, content, link);
		return 1;
	}

	int Parser::parsedCodeSpan(struct buf *ob, struct buf *text) {
		handleBlock(CODE_SPAN, ob, text);
		return 1;
	}

	int Parser::parsedLinebreak(struct buf *ob) {
		handleSpan(LINEBREAK, ob, NULL);
		return 1;
	}

	// Low Level Callbacks

	void Parser::parsedNormalText(struct buf *ob, struct buf *text) {
		// The parser will spuriously emit a text callback for an empty string
		// that butts up against a span-level element. This will ignore it.

		if (text && text->size > 0) {
			Element normalText;
			normalText.setType(TEXT);
			normalText.setText(std::string(text->data).substr(0, text->size));
			createSpan(normalText, ob);
		}
	}

}

// Block Element callbacks

static void rndr_blockcode(struct buf *ob, struct buf *text, void *opaque) {
	((Bypass::Parser*) opaque)->parsedBlockcode(ob, text);
}

static void rndr_blockquote(struct buf *ob, struct buf *text, void *opaque) {
	((Bypass::Parser*) opaque)->parsedBlockquote(ob, text);
}

static void rndr_header(struct buf *ob, struct buf *text, int level, void *opaque) {
	((Bypass::Parser*) opaque)->parsedHeader(ob, text, level);
}

static void rndr_list(struct buf *ob, struct buf *text, int flags, void *opaque) {
	((Bypass::Parser*) opaque)->parsedList(ob, text, flags);
}

static void rndr_listitem(struct buf *ob, struct buf *text, int flags, void *opaque) {
	((Bypass::Parser*) opaque)->parsedListItem(ob, text, flags);
}

static void rndr_paragraph(struct buf *ob, struct buf *text, void *opaque) {
	((Bypass::Parser*) opaque)->parsedParagraph(ob, text);
}

// Span Element callbacks

static int rndr_codespan(struct buf *ob, struct buf *text, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedCodeSpan(ob, text);
}

static int rndr_double_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedDoubleEmphasis(ob, text, c);
}

static int rndr_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedEmphasis(ob, text, c);
}

static int rndr_triple_emphasis(struct buf *ob, struct buf *text, char c, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedTripleEmphasis(ob, text, c);
}

static int rndr_linebreak(struct buf *ob, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedLinebreak(ob);
}

static int rndr_link(struct buf *ob, struct buf *link, struct buf *title, struct buf *content, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedLink(ob, link, title, content);
}

//	Low Level Callbacks

static void rndr_normal_text(struct buf *ob, struct buf *text, void *opaque) {
	return ((Bypass::Parser*) opaque)->parsedNormalText(ob, text);
}

