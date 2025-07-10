#include "kicad.hpp"
#include <fstream>


namespace kicad {

namespace {

// tokens
enum class Token {
    CONTAINER,
    CONTAINER_END,
    VALUE,
    FILE_END,
};


class Tokenizer {
public:
    Tokenizer(std::istream &s) : s(s) {}

    Token getToken() {
        if (!refill())
            return Token::FILE_END;
        char ch = this->buffer[this->pos];
        if (ch == '(')
            return Token::CONTAINER;
        if (ch == ')')
            return Token::CONTAINER_END;
        return Token::VALUE;
    }

    double readNumber() {
        std::string str;
        readString(str);
        return std::stod(str);
    }

    void readContainer(std::string &str) {
        // skip '('
        ++this->pos;
        readString(str);
    }

    void readContainerEnd() {
        // skip ')'
        ++this->pos;
    }

    bool readString(std::string &str) {
        const char *b = this->buffer;
        int p = this->pos;
        int e = this->end;

        char ch = b[p];
        if (ch == '"') {
            // quoted string
            do {
                ++p;
                if (p >= e)
                    break;
                if (b[p] == '\\') {
                    p += 2;
                    if (p >= e)
                        break;
                }
            } while (b[p] != '"');
            str.assign(b + this->pos/* + 1*/, p + 1 - (this->pos/* + 1*/));
            this->pos = p + 1;
            return true;
        } else {
            // identifier
            do {
                ++p;
                if (p >= e)
                    break;
            } while (b[p] != ')' && uint8_t(b[p]) > ' ');
            str.assign(b + this->pos, p - this->pos);
            this->pos = p;
            return false;
        }
    }

protected:
    bool refill() {
        char *b = this->buffer;
        int p = this->pos;
        int e = this->end;
        while (p < e && uint8_t(p[b]) <= ' ') {
            ++p;
        }

        int size = e - p;
        if (size < 1024) {
            memmove(b, b + p, size);

            // read
            s.read(b + size, 2048 - size);
            p = 0;
            e = size + s.gcount();
        }
        this->pos = p;
        this->end = e;
        return p < e;
    }

    std::istream &s;
    char buffer[2048];
    int pos = 0;
    int end = 0;
};

Value *readValue(Tokenizer &t) {
    auto str = new Value();
    /*str->quote = */t.readString(str->value);
    return str;
}

void readContainer(Tokenizer &t, Container &container) {
    t.readContainer(container.id);

    while (true) {
        auto token = t.getToken();
        switch (token) {
        case Token::CONTAINER:
            {
                Container *c = new Container();
                readContainer(t, *c);
                container.elements.push_back(c);
            }
            break;
        case Token::CONTAINER_END:
            t.readContainerEnd();
            return;
        case Token::VALUE:
            container.elements.push_back(readValue(t));
            break;
        case Token::FILE_END:
            return;
        }
    }
}

} // namespace


std::string toString(std::string_view value) {
    std::string str;
    str += '"';
    str += value;
    str += '"';
    return str;
}


// Value

Value::~Value() {
}

int Value::count() {
    return 1;
}

void Value::write(std::ostream &s, int indent) {
    s << this->value;
}


// Container

Container::~Container() {
    clear();
}

void Container::clear() {
    for (auto element : this->elements) {
        delete element;
    }
    this->elements.clear();
}

int Container::count() {
    int c = 1;
    for (auto element : this->elements) {
        c += element->count();
    }
    return c;
}


Container *Container::add(std::string_view id) {
    auto container = new Container(id);
    this->elements.push_back(container);
    return container;
}

Container &Container::addValue(std::string_view value) {
    this->elements.push_back(new Value(value));
    return *this;
}

Container &Container::setTag(int index, std::string_view value) {
    if (index >= this->elements.size())
        this->elements.resize(index + 1);
    this->elements[index] = new Value(value);
    return *this;
}

Container &Container::setString(int index, std::string_view value) {
    if (index >= this->elements.size())
        this->elements.resize(index + 1);
    auto v = new Value();
    v->value += '"';
    v->value += value;
    v->value += '"';
    this->elements[index] = v;
    return *this;
}

Container &Container::setInt(int index, int value) {
    if (index >= this->elements.size())
        this->elements.resize(index + 1);
    this->elements[index] = new Value(std::to_string(value));
    return *this;
}

Container &Container::setFloat(int index, double value) {
    if (index >= this->elements.size())
        this->elements.resize(index + 1);
    this->elements[index] = new Value(std::to_string(value));
    return *this;
}

std::string Container::getTag(int index, std::string_view defaultValue) {
    auto value = getValue(index);
    if (value == nullptr)
        return std::string(defaultValue);

    return value->value;
}

std::string Container::getString(int index, std::string_view defaultValue) {
    auto value = getValue(index);
    if (value == nullptr)
        return std::string(defaultValue);

    // remove quotes
    int size = value->value.size();
    if (size >= 2 && value->value.front() == '"' && value->value.back() == '"')
        return std::string(value->value.substr(1, size - 2));

    return value->value;
}

int Container::getInt(int index, int defaultValue) {
    auto value = getValue(index);
    if (value == nullptr)
        return defaultValue;

    // cast to int
    try {
        return std::stoi(value->value);
    } catch (std::exception &) {
        return defaultValue;
    }
}

double Container::getFloat(int index, double defaultValue) {
    auto value = getValue(index);
    if (value == nullptr)
        return defaultValue;

    // cast to double
    try {
        return std::stod(value->value);
    } catch (std::exception &) {
        return defaultValue;
    }
}


bool Container::contains(std::string_view tag) {
    for (auto element : this->elements) {
        auto v = dynamic_cast<Value *>(element);
        if (v != nullptr) {
            if (v->value == tag)
                return true;
        }
    }
    return false;
}

Container *Container::find(std::string_view id) {
    for (auto element : this->elements) {
        auto container = dynamic_cast<Container *>(element);
        if (container != nullptr) {
            if (container->id == id) {
                return container;
            }
        }
    }
    return nullptr;
}

std::string Container::findString(std::string_view id) {
    auto container = find(id);
    if (container != nullptr)
        return container->getString(0);
    return {};
}

double Container::findFloat(std::string_view id) {
    auto container = find(id);
    if (container != nullptr)
        return container->getFloat(0);
    return {};
}

Container::Value2<std::string> Container::findString2(std::string_view id) {
    auto container = find(id);
    if (container != nullptr)
        return {container->getString(0), container->getString(1)};
    return {};
}

Container::Value2<double> Container::findFloat2(std::string_view id) {
    auto container = find(id);
    if (container != nullptr)
        return {container->getFloat(0), container->getFloat(1)};
    return {};
}

void Container::erase(Element *element) {
    for (auto it = this->elements.begin(); it != this->elements.end(); ++it) {
        if (*it == element) {
            this->elements.erase(it);
            delete element;
            return;
        }
    }
}

void Container::erase(std::string_view id) {
    auto it = this->elements.begin();
    while (it != this->elements.end()) {
        auto container = dynamic_cast<kicad::Container *>(*it);
        if (container->id == id) {
            it = this->elements.erase(it);
            delete container;
        } else {
            ++it;
        }
    }
}

void Container::write(std::ostream &s, int indent) {
    bool multiLine = count() > 16;

    s << '(' << this->id;

    bool multiline2 = false;
    for (auto element : this->elements) {
        multiline2 |= dynamic_cast<Container *>(element) != nullptr;

        if (multiLine && multiline2) {
            newLine(s, indent + 1);
        } else {
            s << ' ';
        }

        element->write(s, indent + 1);
    }
    if (multiLine && multiline2) {
        newLine(s, indent);
    }
    s << ')';
}

void Container::newLine(std::ostream &s, int indent) {
    s << std::endl;
    for (int i = 0; i < indent; ++i) {
        s << "  ";
    }
}



void readFile(std::istream &s, Container &kicad) {
    Tokenizer t(s);

    // file starts with a container
    auto token = t.getToken();
    if (token == Token::CONTAINER)
        readContainer(t, kicad);
}

} // namespace kicad
