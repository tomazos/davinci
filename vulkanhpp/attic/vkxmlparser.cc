#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <tinyxml2.h>

using namespace tinyxml2;

#define DUMP(expr)                                                 \
  do {                                                             \
    std::cout << "DUMP " << #expr << " = " << (expr) << std::endl; \
  } while (0)

#define ASSERT(cond)                          \
  do {                                        \
    if (!(cond)) {                            \
      std::cerr << "!" << #cond << std::endl; \
      std::exit(EXIT_FAILURE);                \
    }                                         \
  } while (0)

template <class... Args>
[[noreturn]] inline void error(Args&&... args) {
  std::ostringstream oss;
  (oss << ... << std::forward<Args>(args));
  throw std::runtime_error(oss.str());
};

template <class... Args>
inline void warn(Args&&... args) {
  std::cerr << "WARNING: ";
  (std::cerr << ... << std::forward<Args>(args));
  std::cerr << std::endl;
};

struct Element;  // fwddecl
struct Node {
 public:
  virtual ~Node() = default;
  bool is_element() const;
  const Element* element() const;
  const std::string& text() const;
  virtual std::string subtext() const = 0;
};

struct Text : Node {
 public:
  Text(const XMLText* xml_text) : text(xml_text->Value()) {}
  const std::string text;
  std::string subtext() const override { return text; }
};

struct Element : public Node {
 public:
  Element(const XMLElement* xml_element);

  const std::string name;
  bool hasattr(const std::string& name) const {
    return attrs.count(std::string(name));
  }
  const std::string& attr(const std::string& name) const {
    return attrs.at(name);
  }

  std::string subelement_name(const std::string& name) const {
    for (const Element* e : subelements)
      if (e->name == name) {
        ASSERT(e->children.size() == 1);
        return e->children.at(0)->text();
      }
    error("no such subelement ", name, " at ", line);
  }
  std::string subtext() const override {
    if (name == "comment") return " ";
    std::ostringstream oss;
    for (const Node* child : children) oss << " " << child->subtext() << " ";
    return oss.str();
  }
  const std::map<std::string, std::string> attrs;
  const std::vector<const Node*> children;
  const std::vector<const Element*> subelements;
  const int line;
};

bool Node::is_element() const { return dynamic_cast<const Element*>(this); }
const Element* Node::element() const {
  return dynamic_cast<const Element*>(this);
}
const std::string& Node::text() const {
  return dynamic_cast<const Text*>(this)->text;
}

std::map<std::string, std::string> to_attr_map(const XMLElement* xml_element) {
  std::map<std::string, std::string> attrs;
  for (auto p = xml_element->FirstAttribute(); p != nullptr; p = p->Next())
    attrs[p->Name()] = p->Value();
  return attrs;
}

std::vector<const Node*> to_children_vec(const XMLElement* xml_element) {
  std::vector<const Node*> children;
  for (auto p = xml_element->FirstChild(); p != nullptr; p = p->NextSibling()) {
    if (auto e = p->ToElement()) {
      children.push_back(new Element(e));
    } else if (auto t = p->ToText()) {
      children.push_back(new Text(t));
    }
  }
  return children;
}

std::vector<const Element*> to_subelement_vec(
    const std::vector<const Node*>& nodes) {
  std::vector<const Element*> subelements;
  for (const Node* n : nodes)
    if (n->is_element()) subelements.push_back(n->element());
  return subelements;
}

bool interp_bool(const std::string& s) {
  if (!(s == "true" || s == "false")) error("interp_bool ", s);
  return s == "true";
}

Element::Element(const XMLElement* xml_element)
    : name(xml_element->Name()),
      attrs(to_attr_map(xml_element)),
      children(to_children_vec(xml_element)),
      subelements(to_subelement_vec(children)),
      line(xml_element->GetLineNum()) {}

void check_attributes(const Element* element,
                      const std::set<std::string>& required_names,
                      const std::set<std::string>& optional_names) {
  for (const std::string& required_name : required_names)
    if (!element->attrs.count(required_name))
      error("required attribute ", required_name, " missing from ",
            element->name);
  for (auto& [name, attribute] : element->attrs) {
    (void)attribute;
    if (!optional_names.count(name) && !required_names.count(name))
      error("unknown attribute ", name, " in ", element->name);
  }
};

struct Entity;

std::map<std::string, Entity*> symbol_table;

void check_exists(const std::string& name) {
  if (symbol_table.count(name) != 1) error("unknown name: ", name);
}

struct Platform {
  std::string name;
  std::string protect;
};

struct Entity {
  std::string name;
  std::vector<std::string> aliases;
  bool referenced = false;
  std::optional<Platform> platform;
  virtual void resolve() {}
  virtual void test_code(std::ostream& o) {
    //    o << "{ constexpr auto x = " << name << "; (void)x; }";
  }
  void set_platform(const Platform& platform_in) {
    if (platform) {
      error("duplicate platform: ", name);
    }
    platform = platform_in;
  }
  virtual ~Entity() = default;
};

void reference_entity(const std::string& name) {
  Entity* entity = symbol_table.at(name);
  entity->referenced = true;
}

struct SpecialEntity : Entity {};

std::string adj(const std::string& name) {
  if (name.substr(0, 3) != "VK_" && name.substr(0, 3) != "vk_")
    error("no vk prefix: ", name);

  return name.substr(3);
}

struct Constant : Entity {
  std::string value;
  virtual void test_code(std::ostream& o) {
    o << "constexpr auto " << adj(name) << " = " << name << ";\n";
  }
};

struct Selector : Entity {
  std::string value;
  virtual void test_code(std::ostream& o) {
    o << "constexpr auto " << adj(name) << " = " << name << ";\n";
  }
};

struct Selection {
  std::string name;
  std::vector<Selector> selectors;
};

struct Flag : Entity {
  std::optional<std::string> value, bitpos;
  virtual void test_code(std::ostream& o) {
    o << "constexpr auto " << adj(name) << " = " << name << ";\n";
  }
};

struct Bitmask {
  std::string name;
  std::vector<Flag> flags;
};

struct Tag {
  std::string name;
  std::string author;
  std::string contact;
};

struct ExternalType : Entity {
  std::string requires;
  void resolve() override {}
  virtual void test_code(std::ostream& o) {
    //    o << "{ using x = " << name << "; x* y; (void)y; }\n";
  }
};

struct Include {
  std::string name;
};

struct Define : Entity {};

struct Basetype : Entity {
  std::string type;
};

struct BitmaskType : Entity {
  std::optional<std::string> requires;
  void resolve() override {
    if (requires) check_exists(*requires);
  }
  virtual void test_code(std::ostream& o) {
    //    o << "{ /*bitmask*/ using x = " << name << "; x* y; (void)y; }\n";
  }
};

struct Alias {
  std::string name;
  std::string target;
};

struct Handle : Entity {
  bool dispatchable;
  std::vector<std::string> parents;
  void resolve() override {
    for (const std::string& parent : parents) check_exists(parent);
  }
  virtual void test_code(std::ostream& o) {
    o << "using " << adj(name) << " = " << name << ";\n";
  }
};

struct EnumType : Entity {
  std::optional<Bitmask> bitmask;
  std::optional<Selection> selection;
  virtual void test_code(std::ostream& o) {
    //    if (platform) o << "#ifdef " << platform->protect << "\n";
    //    o << "{ /*enum*/ using x = " << name << "; x* y; (void)y; }\n";
    //    if (platform) o << "#endif\n";
  }
};

struct TypeId {
  virtual std::string id() const = 0;
  virtual void resolve() const = 0;
  virtual ~TypeId() = default;
};

struct Name : TypeId {
  Name(const std::string& name) : name(name) {}
  std::string id() const override { return name; }
  std::string name;
  void resolve() const override { check_exists(name); }
};

struct Pointer : TypeId {
  Pointer(TypeId* target) : target(target) {}
  std::string id() const override { return target->id() + "*"; }
  TypeId* target;
  void resolve() const override { target->resolve(); }
};

struct PointerToConst : TypeId {
  PointerToConst(TypeId* target) : target(target) {}
  std::string id() const override { return target->id() + " const *"; }
  TypeId* target;
  void resolve() const override { target->resolve(); }
};

struct Token {
  enum Kind { IDENTIFIER, NUMBER, ASTERISK, LBRACKET, RBRACKET, CONST } kind;
  std::string spelling;
};

std::ostream& operator<<(std::ostream& o, Token tok) {
  switch (tok.kind) {
    case Token::IDENTIFIER:
      o << "IDENTIFIER(" << tok.spelling << ")";
      break;
    case Token::NUMBER:
      o << "NUMBER(" << tok.spelling << ")";
      break;
    case Token::ASTERISK:
      o << "ASTERISK";
      break;
    case Token::LBRACKET:
      o << "LBRACKET";
      break;
    case Token::RBRACKET:
      o << "RBRACKET";
      break;
    case Token::CONST:
      o << "CONST";
      break;
  }
  return o;
}

struct Array : TypeId {
  Array(TypeId* target, Token count) : target(target), count(count) {}
  std::string id() const override {
    return target->id() + "[" + count.spelling + "]";
  }
  TypeId* target;
  Token count;
  void resolve() const override {
    target->resolve();
    if (count.kind == Token::IDENTIFIER) check_exists(count.spelling);
  }
};

struct ArrayOfConst : TypeId {
  ArrayOfConst(TypeId* target, Token count) : target(target), count(count) {}
  std::string id() const override {
    return "const " + target->id() + "[" + count.spelling + "]";
  }
  TypeId* target;
  Token count;
  void resolve() const override {
    target->resolve();
    if (count.kind == Token::IDENTIFIER) check_exists(count.spelling);
  }
};

struct Member {
  TypeId* type;
  std::string name;

  std::optional<std::string> values, len, altlen;
  bool optional_ = false, noautovalidity = false, externsync = false;

  void resolve() {
    type->resolve();

    if (values) check_exists(*values);
  }
};

struct Struct : Entity {
  bool is_union;
  bool returnedonly;
  std::vector<std::string> structextends;
  std::vector<Member> members;
  void resolve() override {
    for (auto s : structextends) check_exists(s);
    for (Member& member : members) member.resolve();
  }
  virtual void test_code(std::ostream& o) {
    if (platform) o << "#ifdef " << platform->protect << "\n";
    o << "{ /*struct*/ using x = " << name << "; x* y; (void)y; }\n";
    if (platform) o << "#endif\n";
  }
};

struct FuncPointer : Entity {};

struct Param {
  std::string name;
  TypeId* type;
  std::optional<std::string> len;
  std::optional<std::string> altlen;
  std::vector<bool> optional;
  bool noautovalidity = false;
  std::optional<std::string> externsync;

  void resolve() { type->resolve(); }
};

struct Command : Entity {
  std::vector<std::string> queues, successcodes, errorcodes, cmdbufferlevel;
  std::optional<std::string> pipeline, renderpass, implicitexternsyncparams;

  TypeId* return_type = nullptr;

  std::vector<Param> params;

  void resolve() override {
    for (const std::string& successcode : successcodes)
      check_exists(successcode);

    for (const std::string& errorcode : errorcodes) check_exists(errorcode);

    for (Param& param : params) param.resolve();
  }
  virtual void test_code(std::ostream& o) {
    if (platform) o << "#ifdef " << platform->protect << "\n";

    o << "{ constexpr auto x = " << name << "; (void)x; }\n";

    if (platform) o << "#endif\n";
  }
};

struct ExtensionEnum : Entity {
  std::optional<std::string> alias;
};

struct Feature {
  std::vector<std::string> types;
  std::vector<std::string> commands;
  std::vector<std::string> reference_enums;
  std::vector<ExtensionEnum> extension_enums;
  std::vector<Alias> aliases;
};

struct Extension {
  std::optional<std::string> platform;
  std::vector<std::string> types;
  std::vector<std::string> commands;
  std::vector<std::string> reference_enums;
  std::vector<ExtensionEnum> extension_enums;
  std::vector<Alias> aliases;
};

std::vector<Constant> constants;
std::vector<Selection> selections;
std::vector<Bitmask> bitmasks;
std::map<std::string, Platform> platforms;
std::vector<Tag> tags;
std::vector<ExternalType> external_types;
std::vector<Include> includes;
std::vector<Define> defines;
std::vector<Basetype> basetypes;
std::vector<BitmaskType> bitmask_types;
std::vector<Handle> handles;
std::vector<EnumType> enum_types;
std::vector<Struct> structs;
std::vector<FuncPointer> func_pointers;
std::vector<Command> commands;
std::vector<Selector> selectors;
std::vector<Extension> extensions;
std::vector<Feature> features;

std::vector<Alias> aliases;

void add_alias(std::string_view name, std::string_view target) {
  Alias alias;
  alias.name = name;
  alias.target = target;
  aliases.push_back(alias);
}

std::vector<std::string> split(const std::string& sep,
                               const std::string joined) {
  std::vector<std::string> result;
  size_t pos = 0;
  while (true) {
    size_t next_pos = joined.find(sep, pos);
    if (next_pos == std::string::npos) {
      result.push_back(joined.substr(pos));
      return result;
    }
    result.push_back(joined.substr(pos, next_pos - pos));
    pos = next_pos + sep.size();
  }
}

void process_constant(const std::string& name, const std::string& value) {
  Constant constant;
  constant.name = name;
  constant.value = value;
  constants.push_back(constant);
}

void process_constant_alias(const std::string& name, const std::string& alias) {
  add_alias(name, alias);
}

void process_constants(const Element* constants) {
  for (auto constant : constants->subelements) {
    if (constant->name != "enum") error("Unknown constant ", constant->name);
    std::string name = constant->attr("name");
    bool a = constant->hasattr("alias");
    bool v = constant->hasattr("value");
    if (!a && !v) error("No alias or value for constant ", name);
    if (a && v) error("Both alias and value for constant ", name);
    if (v) {
      process_constant(name, constant->attr("value"));
    } else /*a*/ {
      process_constant_alias(name, constant->attr("alias"));
    }
  }
}

void process_selector(Selection& selection, const Element* selector_xml) {
  check_attributes(selector_xml, {"name"}, {"value", "alias", "comment"});
  std::string name = selector_xml->attr("name");
  bool v = selector_xml->hasattr("value");
  bool a = selector_xml->hasattr("alias");

  if (!v && !a) error("selector ", name, " has neither value nor alias");
  if (v && a) error("selector ", name, " has both value and alias");

  if (v) {
    std::string value = selector_xml->attr("value");
    Selector selector;
    selector.name = name;
    selector.value = value;
    selection.selectors.push_back(selector);
  } else /*a*/ {
    std::string_view target = selector_xml->attr("alias");
    add_alias(name, target);
  }
}

void process_selection(const Element* selection_xml) {
  check_attributes(selection_xml, {"name", "type"}, {"comment"});
  std::string_view name = selection_xml->attr("name");

  Selection selection;
  selection.name = name;
  for (const Element* selector : selection_xml->subelements) {
    if (selector->name == "comment" || selector->name == "unused") continue;
    if (selector->name != "enum")
      error("Unexpected child of enums ", selector->name);
    process_selector(selection, selector);
  }
  selections.push_back(selection);
}

void process_flag(Bitmask& bitmask, const Element* flag_xml) {
  check_attributes(flag_xml, {"name"}, {"value", "bitpos", "alias", "comment"});
  std::string name = flag_xml->attr("name");
  bool b = flag_xml->hasattr("bitpos");
  bool v = flag_xml->hasattr("value");
  bool a = flag_xml->hasattr("alias");

  int i = 0;
  if (b) i++;
  if (v) i++;
  if (a) i++;

  if (i != 1)
    error("flag ", name,
          " does not contain exactly one of bitpos, value or alias");

  if (b) {
    std::string_view bitpos = flag_xml->attr("bitpos");
    Flag flag;
    flag.name = name;
    flag.bitpos = bitpos;
    bitmask.flags.push_back(flag);
  }

  if (v) {
    std::string_view value = flag_xml->attr("value");
    Flag flag;
    flag.name = name;
    flag.value = value;
    bitmask.flags.push_back(flag);
  }

  if (a) {
    std::string_view target = flag_xml->attr("alias");
    add_alias(name, target);
  }
}

void process_bitmask(const Element* bitmask_xml) {
  check_attributes(bitmask_xml, {"name", "type"}, {"comment"});
  std::string_view name = bitmask_xml->attr("name");
  Bitmask bitmask;
  bitmask.name = name;
  for (const Element* flag : bitmask_xml->subelements) {
    if (flag->name == "comment" || flag->name == "unused") continue;
    if (flag->name != "enum") error("Unexpected child of enums ", flag->name);
    process_flag(bitmask, flag);
  }
  bitmasks.push_back(bitmask);
}

void process_enums(const Element* enums) {
  check_attributes(enums, {"name"}, {"comment", "type"});
  std::string_view name = enums->attr("name");
  if (name == "API Constants") {
    process_constants(enums);
    return;
  }
  std::string_view type = enums->attr("type");
  if (type != "enum" && type != "bitmask") error("unknown enums type ", type);
  if (type == "enum") {
    process_selection(enums);
  } else if (type == "bitmask") {
    process_bitmask(enums);
  } else {
    error("unknown enums type");
  }
}

void process_platforms(const Element* platforms_xml) {
  check_attributes(platforms_xml, {}, {"comment"});

  for (const Element* platform_xml : platforms_xml->subelements) {
    check_attributes(platform_xml, {"name", "protect"}, {"comment"});
    std::string name = platform_xml->attr("name");
    std::string protect = platform_xml->attr("protect");
    Platform platform;
    platform.name = name;
    platform.protect = protect;
    ASSERT(platforms.count(platform.name) == 0);
    platforms[platform.name] = platform;
  }
}

void process_tags(const Element* tags_xml) {
  check_attributes(tags_xml, {}, {"comment"});
  for (const Element* tag_xml : tags_xml->subelements) {
    check_attributes(tag_xml, {"name", "author", "contact"}, {"comment"});
    std::string name = tag_xml->attr("name");
    std::string author = tag_xml->attr("author");
    std::string contact = tag_xml->attr("contact");
    Tag tag;
    tag.name = name;
    tag.author = author;
    tag.contact = contact;
    tags.push_back(tag);
  }
}
// decl:
//   decl-specifiers declarator
//
// declarator:
//

void parse_decl(const std::vector<Token>& tokens, TypeId*& type,
                std::string& name) {
  Member member;
  //  size_t pos = 0;
  if (tokens.size() == 2 && tokens.at(0).kind == Token::IDENTIFIER &&
      tokens.at(1).kind == Token::IDENTIFIER) {
    type = new Name(tokens.at(0).spelling);
    name = tokens.at(1).spelling;
    return;
  }
  if (tokens.size() == 3 && tokens.at(0).kind == Token::IDENTIFIER &&
      tokens.at(1).kind == Token::ASTERISK &&
      tokens.at(2).kind == Token::IDENTIFIER) {
    type = new Pointer(new Name(tokens.at(0).spelling));
    name = tokens.at(2).spelling;
    return;
  }
  if (tokens.size() == 4 && tokens.at(0).kind == Token::CONST &&
      tokens.at(1).kind == Token::IDENTIFIER &&
      tokens.at(2).kind == Token::ASTERISK &&
      tokens.at(3).kind == Token::IDENTIFIER) {
    type = new PointerToConst(new Name(tokens.at(1).spelling));
    name = tokens.at(3).spelling;
    return;
  }
  if (tokens.size() == 4 && tokens.at(0).kind == Token::IDENTIFIER &&
      tokens.at(1).kind == Token::ASTERISK &&
      tokens.at(2).kind == Token::ASTERISK &&
      tokens.at(3).kind == Token::IDENTIFIER) {
    type = new Pointer(new Pointer(new Name(tokens.at(0).spelling)));
    name = tokens.at(3).spelling;
    return;
  }
  if (tokens.size() == 5 && tokens.at(0).kind == Token::IDENTIFIER &&
      tokens.at(1).kind == Token::IDENTIFIER &&
      tokens.at(2).kind == Token::LBRACKET &&
      tokens.at(4).kind == Token::RBRACKET) {
    ASSERT(tokens.at(3).kind == Token::IDENTIFIER ||
           tokens.at(3).kind == Token::NUMBER);
    type = new Array(new Name(tokens.at(0).spelling), tokens.at(3));
    name = tokens.at(1).spelling;
    return;
  }
  if (tokens.size() == 6 && tokens.at(0).kind == Token::CONST &&
      tokens.at(1).kind == Token::IDENTIFIER &&
      tokens.at(2).kind == Token::IDENTIFIER &&
      tokens.at(3).kind == Token::LBRACKET &&
      tokens.at(5).kind == Token::RBRACKET) {
    ASSERT(tokens.at(4).kind == Token::IDENTIFIER ||
           tokens.at(4).kind == Token::NUMBER);
    type = new ArrayOfConst(new Name(tokens.at(1).spelling), tokens.at(4));
    name = tokens.at(2).spelling;
    return;
  }

  if (tokens.size() == 6 && tokens.at(0).kind == Token::CONST &&
      tokens.at(1).kind == Token::IDENTIFIER &&
      tokens.at(2).kind == Token::ASTERISK &&
      tokens.at(3).kind == Token::CONST &&
      tokens.at(4).kind == Token::ASTERISK &&
      tokens.at(5).kind == Token::IDENTIFIER) {
    type =
        new PointerToConst(new PointerToConst(new Name(tokens.at(1).spelling)));
    name = tokens.at(5).spelling;
    return;
  }

  error("Unknown token pattern");
}

void parse_decl(const std::string& text, TypeId*& type, std::string& name) {
  std::vector<Token> tokens;
  size_t pos = 0;
  while (true) {
    auto getchar = [text](size_t i) {
      return i < text.size() ? text.at(i) : -1;
    };
    int c = getchar(pos);
    //    int la = getchar(pos + 1);
    if (c == -1) {
      parse_decl(tokens, type, name);
      return;
    }
    if (std::isspace(c)) {
      pos++;
      continue;
    }
    if (std::isalpha(c) || c == '_') {
      size_t eoi;
      auto isidchar = [](char c) { return std::isalnum(c) || c == '_'; };
      for (eoi = pos + 1; isidchar(getchar(eoi)); eoi++)
        ;
      std::string identifier = text.substr(pos, eoi - pos);
      if (identifier != "struct") {
        Token tok;
        tok.kind = (identifier == "const" ? Token::CONST : Token::IDENTIFIER);
        tok.spelling = identifier;
        tokens.push_back(tok);
      }
      pos = eoi;
      continue;
    }
    if (std::isdigit(c)) {
      size_t eoi;
      for (eoi = pos + 1; std::isdigit(getchar(eoi)); eoi++)
        ;
      std::string number = text.substr(pos, eoi - pos);
      Token tok;
      tok.kind = Token::NUMBER;
      tok.spelling = number;
      tokens.push_back(tok);
      pos = eoi;
      continue;
    }
    Token tok;
    switch (c) {
      case '*':
        tok.kind = Token::ASTERISK;
        break;
      case '[':
        tok.kind = Token::LBRACKET;
        break;
      case ']':
        tok.kind = Token::RBRACKET;
        break;
      default:
        DUMP(text.at(pos));
        error(text.at(pos));
    }
    tokens.push_back(tok);
    pos++;
  }
}

void process_types(const Element* types_xml) {
  check_attributes(types_xml, {}, {"comment"});
  for (const Element* type_xml : types_xml->subelements) {
    if (type_xml->name == "comment") continue;
    ASSERT(type_xml->name == "type");
    check_attributes(type_xml, {},
                     {"name", "comment", "category", "requires", "alias",
                      "parent", "returnedonly", "structextends"});
    bool has_name = type_xml->hasattr("name");
    bool has_category = type_xml->hasattr("category");
    bool has_requires = type_xml->hasattr("requires");
    bool has_alias = type_xml->hasattr("alias");
    bool has_parent = type_xml->hasattr("parent");
    bool has_returnedonly = type_xml->hasattr("returnedonly");
    bool has_structextends = type_xml->hasattr("structextends");
    std::string fingerprint =
        std::string() + (has_name ? "n" : "") + (has_category ? "c" : "") +
        (has_requires ? "r" : "") + (has_alias ? "a" : "") +
        (has_parent ? "p" : "") + (has_returnedonly ? "R" : "") +
        (has_structextends ? "s" : "");
    if (fingerprint == "nr") {
      ASSERT(type_xml->children.empty());
      ExternalType external_type;
      external_type.name = type_xml->attr("name");
      external_type.requires = type_xml->attr("requires");
      external_types.push_back(external_type);
    } else if (fingerprint == "n") {
      ASSERT(type_xml->children.empty());
      ExternalType external_type;
      external_type.name = type_xml->attr("name");
      external_types.push_back(external_type);
    } else {
      std::string category = type_xml->attr("category");
      if (category == "include") {
        ASSERT(fingerprint == "nc");
        Include include;
        include.name = type_xml->attr("name");
        includes.push_back(include);
      } else if (category == "define") {
        std::string name;
        if (has_name) {
          ASSERT(fingerprint == "nc");
          name = type_xml->attr("name");
        } else {
          ASSERT(fingerprint == "c");
          for (const Element* element : type_xml->subelements) {
            if (element->name == "name") {
              ASSERT(element->children.size() == 1);
              name = element->children.at(0)->text();
            }
          }
        }
        ASSERT(!name.empty());
        Define define;
        define.name = name;
        defines.push_back(define);
      } else if (category == "basetype") {
        ASSERT(fingerprint == "c");
        Basetype basetype;
        basetype.name = type_xml->subelement_name("name");
        basetype.type = type_xml->subelement_name("type");
        basetypes.push_back(basetype);
      } else if (category == "bitmask") {
        if (!has_alias) {
          BitmaskType bitmask_type;
          bitmask_type.name = type_xml->subelement_name("name");
          if (has_requires) bitmask_type.requires = type_xml->attr("requires");
          bitmask_types.push_back(bitmask_type);
          std::string type = type_xml->subelement_name("type");
          ASSERT(type == "VkFlags");
        } else {
          ASSERT(fingerprint == "nca");
          add_alias(type_xml->attr("name"), type_xml->attr("alias"));
        }
      } else if (category == "handle") {
        if (fingerprint == "nca") {
          add_alias(type_xml->attr("name"), type_xml->attr("alias"));
        } else {
          std::string type = type_xml->subelement_name("type");
          ASSERT(type == "VK_DEFINE_NON_DISPATCHABLE_HANDLE" ||
                 type == "VK_DEFINE_HANDLE");
          Handle handle;
          handle.name = type_xml->subelement_name("name");
          handle.dispatchable = (type == "VK_DEFINE_HANDLE");
          if (fingerprint == "cp") {
            std::string parents = type_xml->attr("parent");
            split(",", parents);
          } else
            ASSERT(fingerprint == "c");
          handles.push_back(handle);
        }
      } else if (category == "enum") {
        if (fingerprint == "nc") {
          EnumType enum_type;
          enum_type.name = type_xml->attr("name");
          enum_types.push_back(enum_type);
        } else if (fingerprint == "nca") {
          add_alias(type_xml->attr("name"), type_xml->attr("alias"));
        } else {
          error("Unknown enum fingerprint: ", fingerprint);
        }
      } else if (category == "struct" || category == "union") {
        if (fingerprint == "nca") {
          add_alias(type_xml->attr("name"), type_xml->attr("alias"));
        } else if (fingerprint == "nc" || fingerprint == "ncR" ||
                   fingerprint == "ncs" || fingerprint == "ncRs") {
          Struct struct_;
          struct_.is_union = category == "union";
          struct_.name = type_xml->attr("name");
          struct_.returnedonly = false;
          if (has_returnedonly) {
            ASSERT(type_xml->attr("returnedonly") == "true");
            struct_.returnedonly = true;
          }
          if (has_structextends) {
            struct_.structextends = split(",", type_xml->attr("structextends"));
          }
          for (const Node* node : type_xml->children) {
            ASSERT(node->is_element());
            const Element* member_xml = node->element();
            if (member_xml->name == "comment") continue;
            if (member_xml->name != "member") error(node->element()->name);
            check_attributes(member_xml, {},
                             {"comment", "noautovalidity", "values", "len",
                              "optional", "altlen", "externsync"});
            std::string decl_text = node->subtext();
            Member member;
            parse_decl(decl_text, member.type, member.name);

            if (member_xml->hasattr("noautovalidity"))
              member.noautovalidity =
                  interp_bool(member_xml->attr("noautovalidity"));

            if (member_xml->hasattr("values")) {
              ASSERT(member.type->id() == "VkStructureType" &&
                     member.name == "sType");
              member.values = member_xml->attr("values");
            }
            if (member_xml->hasattr("len"))
              member.len = member_xml->attr("len");
            if (member_xml->hasattr("altlen"))
              member.altlen = member_xml->attr("altlen");
            if (member_xml->hasattr("optional"))
              member.optional_ = interp_bool(member_xml->attr("optional"));
            if (member_xml->hasattr("externsync"))
              member.externsync = interp_bool(member_xml->attr("externsync"));
            struct_.members.push_back(member);
          }
          structs.push_back(struct_);
        } else {
          error("Unknown struct fingerprint: ", fingerprint);
        }
      } else if (category == "funcpointer") {
        FuncPointer func_pointer;
        func_pointer.name = type_xml->subelement_name("name");
        func_pointers.push_back(func_pointer);
      } else {
        error("unknown type category: ", category);
      }
    }
  }
}

void process_commands(const Element* commands_xml) {
  check_attributes(commands_xml, {}, {"comment"});

  for (const Element* command_xml : commands_xml->subelements) {
    check_attributes(
        command_xml, {},
        {"comment", "queues", "successcodes", "errorcodes", "cmdbufferlevel",
         "renderpass", "pipeline", "alias", "name"});

    if (command_xml->hasattr("alias")) {
      check_attributes(command_xml, {"name", "alias"}, {"comment"});
      add_alias(command_xml->attr("name"), command_xml->attr("alias"));
    } else {
      check_attributes(command_xml, {},
                       {"comment", "queues", "successcodes", "errorcodes",
                        "cmdbufferlevel", "renderpass", "pipeline"});
      Command command;
      if (command_xml->hasattr("queues"))
        command.queues = split(",", command_xml->attr("queues"));

      if (command_xml->hasattr("successcodes"))
        command.successcodes = split(",", command_xml->attr("successcodes"));

      if (command_xml->hasattr("errorcodes"))
        command.errorcodes = split(",", command_xml->attr("errorcodes"));

      if (command_xml->hasattr("cmdbufferlevel"))
        command.cmdbufferlevel =
            split(",", command_xml->attr("cmdbufferlevel"));

      if (command_xml->hasattr("renderpass"))
        command.renderpass = command_xml->attr("renderpass");

      if (command_xml->hasattr("pipeline"))
        command.pipeline = command_xml->attr("pipeline");

      for (const Element* subcommand_xml : command_xml->subelements) {
        if (subcommand_xml->name == "proto") {
          check_attributes(subcommand_xml, {}, {});
          ASSERT(command.return_type == nullptr);
          parse_decl(subcommand_xml->subtext(), command.return_type,
                     command.name);
        } else if (subcommand_xml->name == "param") {
          Param param;
          check_attributes(
              subcommand_xml, {},
              {"len", "altlen", "optional", "noautovalidity", "externsync"});
          if (subcommand_xml->hasattr("len"))
            param.len = subcommand_xml->attr("len");
          if (subcommand_xml->hasattr("altlen"))
            param.altlen = subcommand_xml->attr("altlen");
          if (subcommand_xml->hasattr("optional"))
            for (std::string s : split(",", subcommand_xml->attr("optional")))
              param.optional.push_back(interp_bool(s));
          if (subcommand_xml->hasattr("noautovalidity"))
            param.noautovalidity =
                interp_bool(subcommand_xml->attr("noautovalidity"));
          if (subcommand_xml->hasattr("externsync"))
            param.externsync = subcommand_xml->attr("externsync");
          parse_decl(subcommand_xml->subtext(), param.type, param.name);
          command.params.push_back(param);
        } else if (subcommand_xml->name == "implicitexternsyncparams") {
          command.implicitexternsyncparams = subcommand_xml->subtext();
        } else {
          error("unknown subcommand ", subcommand_xml->name);
        }
      }
      ASSERT(command.return_type != nullptr);
      commands.push_back(command);
    }
  }
}

template <typename Container>
void process_require(Container& container, const Element* container_xml) {
  for (const Element* require : container_xml->subelements) {
    ASSERT(require->name == "require");
    check_attributes(require, {}, {"extension", "feature", "comment"});
    for (const Element* subrequire : require->subelements) {
      if (subrequire->name == "comment") continue;
      std::string name = subrequire->attr("name");
      if (subrequire->name == "enum") {
        if (subrequire->hasattr("value") || subrequire->hasattr("bitpos") ||
            subrequire->hasattr("alias") || subrequire->hasattr("offset")) {
          ExtensionEnum extension_enum;
          extension_enum.name = name;
          if (subrequire->hasattr("alias"))
            extension_enum.alias = subrequire->attr("alias");
          container.extension_enums.push_back(extension_enum);
        } else {
          container.reference_enums.push_back(name);
          // reference enum
        }
      } else if (subrequire->name == "type") {
        container.types.push_back(name);
      } else if (subrequire->name == "command") {
        container.commands.push_back(name);
      } else {
        error("subrequire ", subrequire->name);
      }
    }
  }
}

void process_feature(const Element* feature_xml) {
  check_attributes(feature_xml, {"api", "name", "number"}, {"comment"});

  Feature feature;
  process_require(feature, feature_xml);
  features.push_back(feature);
}

void process_extensions(const Element* extensions_xml) {
  check_attributes(extensions_xml, {}, {"comment"});

  for (const Element* extension_xml : extensions_xml->subelements) {
    ASSERT(extension_xml->name == "extension");
    check_attributes(extension_xml, {"name", "number", "supported"},
                     {"requires", "comment", "platform", "requiresCore",
                      "deprecatedby", "type", "promotedto", "obsoletedby",
                      "contact", "author", "provisional"});

    //    if (extension_xml->attr("supported") == "disabled") continue;

    Extension extension;
    if (extension_xml->hasattr("platform"))
      extension.platform = extension_xml->attr("platform");
    process_require(extension, extension_xml);
    extensions.push_back(extension);
  }
}

int main(int argc, char** argv) {
  try {
    std::vector<std::string> args(argv + 1, argv + argc);
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLError e = doc.LoadFile(args.at(0).c_str());
    if (e != tinyxml2::XML_SUCCESS) error("Unable to parse ", args.at(0));

    Element registry(doc.RootElement());

    ASSERT(registry.name == "registry");
    ASSERT(registry.attrs.empty());
    for (const Element* registry_entry : registry.subelements) {
      std::string n = registry_entry->name;
      if (n == "enums") {
        process_enums(registry_entry);
      } else if (n == "comment") {
        /*pass*/
      } else if (n == "platforms") {
        process_platforms(registry_entry);
      } else if (n == "tags") {
        process_tags(registry_entry);
      } else if (n == "types") {
        process_types(registry_entry);
      } else if (n == "commands") {
        process_commands(registry_entry);
      } else if (n == "feature") {
        process_feature(registry_entry);
      } else if (n == "extensions") {
        process_extensions(registry_entry);
      } else {
        error("unknown registry_entry ", n);
      }
    }

    auto enter_symbol = [&](Entity& entity) {
      if (symbol_table.count(entity.name) != 0) {
        warn("duplicate symbol: ", entity.name, " ", typeid(entity).name());
        return;
      }
      symbol_table[entity.name] = &entity;
    };

    SpecialEntity vk_platform;
    vk_platform.name = "vk_platform";

    enter_symbol(vk_platform);

    for (Constant& constant : constants) enter_symbol(constant);

    for (ExternalType& external_type : external_types)
      enter_symbol(external_type);

    for (Define& define : defines) enter_symbol(define);

    for (Basetype& basetype : basetypes) enter_symbol(basetype);

    for (BitmaskType& empty_bitmask_type : bitmask_types) {
      enter_symbol(empty_bitmask_type);
    }

    for (EnumType& enum_type : enum_types) enter_symbol(enum_type);

    for (Bitmask& bitmask : bitmasks) {
      Entity* entity = symbol_table.at(bitmask.name);
      EnumType& enum_type = dynamic_cast<EnumType&>(*entity);
      enum_type.bitmask = bitmask;
      for (Flag& flag : bitmask.flags) enter_symbol(flag);
    }

    for (Selection& selection : selections) {
      Entity* entity = symbol_table.at(selection.name);
      EnumType& enum_type = dynamic_cast<EnumType&>(*entity);
      enum_type.selection = selection;
      for (Selector& selector : selection.selectors) enter_symbol(selector);
    }

    for (Handle& handle : handles) enter_symbol(handle);

    for (Struct& struct_ : structs) enter_symbol(struct_);

    for (FuncPointer& func_pointer : func_pointers) enter_symbol(func_pointer);

    for (Command& command : commands) enter_symbol(command);

    for (Alias& alias : aliases) {
      Entity* entity = symbol_table.at(alias.target);
      entity->aliases.push_back(alias.name);
      ASSERT(symbol_table.count(alias.name) == 0);
      symbol_table[alias.name] = entity;
    }

    for (Feature& feature : features)
      for (ExtensionEnum& extension_enum : feature.extension_enums)
        enter_symbol(extension_enum);

    for (Extension& extension : extensions)
      for (ExtensionEnum& extension_enum : extension.extension_enums)
        enter_symbol(extension_enum);

    for (auto& kv : symbol_table) kv.second->resolve();

    for (Feature& feature : features) {
      for (const std::string& type : feature.types) reference_entity(type);

      for (const std::string& command : feature.commands)
        reference_entity(command);

      for (const std::string& reference_enum : feature.reference_enums)
        reference_entity(reference_enum);

      for (ExtensionEnum& extension_enum : feature.extension_enums)
        if (extension_enum.alias) reference_entity(*extension_enum.alias);
    }

    for (Extension& extension : extensions) {
      for (const std::string& type : extension.types) reference_entity(type);

      for (const std::string& command : extension.commands)
        reference_entity(command);

      for (const std::string& reference_enum : extension.reference_enums)
        reference_entity(reference_enum);

      for (ExtensionEnum& extension_enum : extension.extension_enums)
        if (extension_enum.alias) reference_entity(*extension_enum.alias);
    }

    auto set_platform_entity = [&](const std::string& name,
                                   const Platform& platform) {
      symbol_table.at(name)->set_platform(platform);
    };

    for (Extension& extension : extensions) {
      if (!extension.platform) continue;
      const Platform& platform = platforms.at(*extension.platform);
      for (const std::string& type : extension.types)
        set_platform_entity(type, platform);

      for (const std::string& command : extension.commands)
        set_platform_entity(command, platform);
    }

    //    for (Command& command : commands) {
    //      if (command.params.empty()) continue;
    //
    //      if (command.params[0].type->id() == "VkInstance")
    //        if (!command.platform)
    //          std::cout << "vkInstance command: " << command.name <<
    //          std::endl;
    //    }

    //        for (auto& kv : symbol_table)
    //          if (!kv.second->referenced)
    //            warn("Unreferenced entity: ", kv.second->name);

    std::ofstream ofs("/home/zos/Projects/davinci/vulkanhpp/t.cc");
    ofs << "#include <vulkan/vulkan.h>\n";
    ofs << "namespace vkx {\n";
    for (auto& kv : symbol_table) {
      kv.second->test_code(ofs);
    }
    ofs << "} // namespace vkx\n";

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
