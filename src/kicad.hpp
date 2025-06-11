#pragma once

#include <istream>
#include <ostream>
#include <list>
#include <map>
#include <string>
#include <vector>


/// @brief classes and functions for reading and writing kicad files
///
namespace kicad {

std::string toString(std::string_view value);


class Element {
public:

    virtual ~Element() {}

    virtual int count() = 0;

    virtual void write(std::ostream &s, int indent) = 0;
};


class Value : public Element {
public:
    Value() = default;
    Value(std::string_view value) : value(value) {}

    virtual ~Value();

    int count() override;

    void write(std::ostream &s, int indent) override;

    std::string value;
};


class Container : public Element {
public:
    struct Value2 {
        std::string a;
        std::string b;
    };


    Container() = default;
    Container(std::string_view id) : id(id) {}

    virtual ~Container();

    void clear();

    int count() override;

    void add(Element *element) {this->elements.push_back(element);}


    Container &addValue(std::string_view value);

    Container &addValue(int value) {
        return addValue(std::to_string(value));
    }

    Container &addValue(double value) {
        return addValue(std::to_string(value));
    }

    template <typename T>
    Container &addValues(const T &value) {
        return addValue(value);
    }

    template <typename T, typename ...Args>
    Container &addValues(const T &value, Args ...args) {
        addValue(value);
        return addValues(args...);
    }

    /// @brief Add a new container
    /// @param id id of container
    /// @return the new container
    Container *add(std::string_view id);

    template <typename T>
    Container *add(std::string_view id, const T &value) {
        auto container = add(id);
        container->addValue(value);
        return container;
    }

    template <typename T, typename ...Args>
    Container *add(std::string_view id, const T &value, Args ...args) {
        auto container = add(id);
        container->addValue(value).addValues(args...);
        return container;
    }

    /// @brief Get a value at given index. If the element does not exist or is not of type Value, an empty string is returned
    /// @param index
    /// @return value at given index
    std::string getValue(int index, const std::string &defaultValue = {}) {
        if (index >= this->elements.size())
            return defaultValue;
        auto value = dynamic_cast<Value *>(this->elements[index]);
        if (!value)
            return defaultValue;
        return value->value;
    }

    void write(std::ostream &s, int indent) override;

    /// @brief Check if the container contains a value
    /// @param value to find
    /// @return true if value is an element of the container
    bool contains(const std::string &value);

    /// @brief Find element container with given id
    /// @param id id of sub-container to find
    /// @return container or nullptr if not found or not of type Container
    Container *find(const std::string &id);

    /// @brief Find element container with given id and return its first value (of type kicad::Value)
    /// @param id id of sub-container to find
    /// @return value or empty string if not found
    std::string findValue(const std::string &id);

    /// @brief Find element container with given id and return its first and second value (of type kicad::Value)
    /// @param id id of sub-container to find
    /// @return values or empty strings if not found
    Value2 findValue2(const std::string &id);

    /// @brief Erase element
    /// @param element Element to erase
    void erase(Element *element);

    static void newLine(std::ostream &s, int indent);

    std::string id;
    std::vector<Element *> elements;
};


/// @brief Read a kicad file
/// @param buffer buffer of an open file or network socket that is in ready state
void readFile(std::istream &s, Container &kicad);

/// @brief Write a kicad file
/// @param buffer buffer of an open file or network socket that is in ready state
inline void writeFile(std::ostream &s, Container &kicad) {
    kicad.write(s, 0);
}

} // namespace kicad
