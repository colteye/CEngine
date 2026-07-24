#include "XMLNodeHandlerHead.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/URL.h"
#include "../../Include/RmlUi/Core/XMLParser.h"
#include "DocumentHeader.h"
#include "TemplateCache.h"

namespace Rml {

static String Absolutepath(const String& source, const String& base)
{
	String joined_path;
	::Rml::GetSystemInterface()->JoinPath(joined_path, StringUtilities::Replace(base, '|', ':'), StringUtilities::Replace(source, '|', ':'));
	return StringUtilities::Replace(joined_path, ':', '|');
}

static DocumentHeader::Resource MakeInlineResource(XMLParser* parser, const String& data)
{
	DocumentHeader::Resource resource;
	resource.is_inline = true;
	resource.content = data;
	resource.path = parser->GetSourceURL().GetURL();
	resource.line = parser->GetLineNumberOpenTag();
	return resource;
}

static DocumentHeader::Resource MakeExternalResource(XMLParser* parser, const String& path)
{
	DocumentHeader::Resource resource;
	resource.is_inline = false;
	resource.path = Absolutepath(path, parser->GetSourceURL().GetURL());
	return resource;
}

XMLNodeHandlerHead::XMLNodeHandlerHead() {}

XMLNodeHandlerHead::~XMLNodeHandlerHead() {}

Element* XMLNodeHandlerHead::ElementStart(XMLParser* parser, const String& name, const XMLAttributes& attributes)
{
	if (name == "head")
	{
		// Process the head attribute
		parser->GetDocumentHeader()->source = parser->GetSourceURL().GetURL();
	}

	// Is it a link tag?
	else if (name == "link")
	{
		// CEngine accepts the ordinary HTML stylesheet link form. The type
		// attribute is optional in HTML and, when present, must name CSS.
		String rel = StringUtilities::ToLower(Get<String>(attributes, "rel", ""));
		String type = StringUtilities::ToLower(Get<String>(attributes, "type", ""));
		String href = Get<String>(attributes, "href", "");
		const bool css_path = href.size() > 4 && href.compare(href.size() - 4, 4, ".css") == 0;

		if (rel == "stylesheet" && (type.empty() || type == "text/css") && css_path)
		{
			parser->GetDocumentHeader()->rcss.push_back(MakeExternalResource(parser, href));
		}
		else
		{
			Log::ParseError(parser->GetSourceURL().GetURL(), parser->GetLineNumber(),
				"Link tag requires rel=\"stylesheet\", a .css href, and optional type=\"text/css\"");
		}
	}

	// Process script tags
	else if (name == "script")
	{
		// Check if its an external string
		String src = Get<String>(attributes, "src", "");
		if (src.size() > 0)
		{
			parser->GetDocumentHeader()->scripts.push_back(MakeExternalResource(parser, src));
		}
	}

	// No elements constructed
	return nullptr;
}

bool XMLNodeHandlerHead::ElementEnd(XMLParser* parser, const String& name)
{
	// When the head tag closes, inject the header into the active document
	if (name == "head")
	{
		Element* element = parser->GetParseFrame()->element;
		if (!element)
			return true;

		ElementDocument* document = element->GetOwnerDocument();
		if (document)
			document->ProcessHeader(parser->GetDocumentHeader());
	}
	return true;
}

bool XMLNodeHandlerHead::ElementData(XMLParser* parser, const String& data, XMLDataType /*type*/)
{
	const String& tag = parser->GetParseFrame()->tag;

	// Store the title
	if (tag == "title")
	{
		SystemInterface* system_interface = GetSystemInterface();
		if (system_interface != nullptr)
			system_interface->TranslateString(parser->GetDocumentHeader()->title, data);
	}

	// Store an inline script
	if (tag == "script" && data.size() > 0)
	{
		parser->GetDocumentHeader()->scripts.push_back(MakeInlineResource(parser, data));
	}

	// Store an inline style
	if (tag == "style" && data.size() > 0)
	{
		parser->GetDocumentHeader()->rcss.push_back(MakeInlineResource(parser, data));
	}

	return true;
}

} // namespace Rml
