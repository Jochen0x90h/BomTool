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

//std::string toString(std::string_view value);


class Element {
public:
    enum class Action {
        NONE,
        KEEP,
        DELETE,
    };

    Element() = default;
    Element(Element &&other) = default;
    virtual ~Element();
    virtual int count() = 0;
    virtual Action sweep();
    virtual void write(std::ostream &s, int indent) = 0;

    Action action = Action::NONE;
};


class Value : public Element {
public:
    Value() = default;
    Value(std::string_view value) : value(value) {}
    Value(Value &&other) = default;

    ~Value() override;
    int count() override;
    void write(std::ostream &s, int indent) override;

    std::string getString(std::string_view defaultValue = {});

    void setString(std::string_view value);


    std::string value;
};


class Container : public Element {
public:
    template <typename T>
    struct Value2 {
        T x;
        T y;
    };


    Container() = default;
    Container(std::string_view id) : id(id) {}
    Container(const Container &) = delete;
    Container(Container &&other) = default;

    virtual ~Container();
    int count() override;
    Action sweep() override;
    void write(std::ostream &s, int indent) override;



    void clear();


    /// @brief Add a new element
    /// @param element
    void add(Element *element) {this->elements.push_back(element);}



/*

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
    /// @param id Id of container
    /// @return The new container
    Container *add(std::string_view id);

    /// @brief Add a new container
    /// @tparam T Type of value to add
    /// @param id Id of container
    /// @param value Value to add
    /// @return The new container
    template <typename T>
    Container *add(std::string_view id, const T &value) {
        auto container = add(id);
        container->addValue(value);
        return container;
    }

    / *template <typename T, typename ...Args>
    Container *add(std::string_view id, const T &value, Args ...args) {
        auto container = add(id);
        container->addValue(value).addValues(args...);
        return container;
    }* /
    template <typename ...Args>
    Container *add(std::string_view id, Args ...args) {
        auto container = add(id);
        container->addValues(args...);
        return container;
    }
*/
    /// @brief Add a new container
    /// @param id Id of container
    /// @return The new container
    Container *add(std::string_view id);

    /// @brief Add a value to the container
    /// @param value Value to add
    /// @return *this
    Container &addValue(std::string_view value);

    Container &addTag(std::string_view value) {return setTag(this->elements.size(), value);}
    Container &addString(std::string_view value) {return setString(this->elements.size(), value);}
    Container &addNumber(double value) {return setNumber(this->elements.size(), value);}

    /// @brief Set a tag at a given index
    /// @param index Index of the element to set
    /// @param value Tag to set
    Container &setTag(int index, std::string_view value);

    /// @brief Set a string at a given index
    /// @param index Index of the element to set
    /// @param value String to set, gets quoted
    Container &setString(int index, std::string_view value);

    /// @brief Set a number at a given index
    /// @param index Index of the element to set
    /// @param value Floating point value to set
    Container &setNumber(int index, double value);


    /// @brief Get an element of type kicad::Value at a given index.
    /// @param index Index
    /// @return Element if the element exists and is of type kicad::Value, otherwise nullptr
    Value *getValue(int index) {
        // check index
        if (unsigned(index) >= this->elements.size())
            return nullptr;

        // check if element is of type Value
        return dynamic_cast<Value *>(this->elements[index]);
    }

    /// @brief Get a tag at given index (string without quotes ).
    /// @param index Index of the element to get
    /// @param defaultValue Value to return if index is out of bounds or element is not of type Value
    /// @return tag at given index
    std::string getTag(int index, std::string_view defaultValue = {});

    /// @brief Get a string at given index, removing quotes if there are any.
    /// @param index Index of the element to get
    /// @param defaultValue Value to return if index is out of bounds or element is not of type Value
    /// @return string value at given index
    std::string getString(int index, std::string_view defaultValue = {});

    /// @brief Get an integer at given index, returning defaultValue if index is out of bounds or element is not of type Value.
    /// @param index Index of the element to get
    /// @param defaultValue Value to return if index is out of bounds or element is not of type Value
    /// @return Integer value at given index
    int getInt(int index, int defaultValue = 0);

    /// @brief Get a number at given index, returning defaultValue if index is out of bounds or element is not of type Value.
    /// @param index Index of the element to get
    /// @param defaultValue Value to return if index is out of bounds or element is not of type Value
    /// @return Number value at given index
    double getNumber(int index, double defaultValue = 0.0);


    /// @brief Check if the container contains a tag.
    /// @param tag Tag to find
    /// @return true if the tag is an element of the container
    bool contains(std::string_view tag);

    /// @brief Find element container with given id.
    /// @param id id of sub-container to find
    /// @return container or nullptr if not found or not of type Container
    Container *find(std::string_view id);

    /// @brief Find element container with given id and return its first value as string.
    /// @param id id of sub-container to find
    /// @return value or empty string if not found
    std::string findString(std::string_view id);

    /// @brief Find element container with given id and return its first value as number.
    /// @param id id of sub-container to find
    /// @return value or zero if not found
    double findNumber(std::string_view id);

    /// @brief Find element container with given id and return its first and second value as string.
    /// @param id id of sub-container to find
    /// @return values or empty strings if not found
    Value2<std::string> findString2(std::string_view id);

    /// @brief Find element container with given id and return its first and second value as number.
    /// @param id id of sub-container to find
    /// @return values or zeros if not found
    Value2<double> findNumber2(std::string_view id);

    /// @brief Erase element.
    /// @param element Element to erase
    void erase(Element *element);

    /// @brief Erase element by id.
    /// @param element Element to erase
    void erase(std::string_view id);


    static void newLine(std::ostream &s, int indent);



    class Iterator {
    public:
        Iterator(std::vector<Element *>::iterator it, std::vector<Element *>::iterator end)
            : it(it), end(end)
        {
            nextContainer();
        }

        Iterator(std::vector<Element *>::iterator end)
            : it(end), end(end)
        {
        }

        Iterator & operator ++() {
            ++this->it;
            nextContainer();
            return *this;
        }

        Container * operator *() {
            return static_cast<Container *>(*this->it);
        }

        bool operator == (const Iterator &other) const {
            return this->it == other.it;
        }

    protected:
        void nextContainer() {
            while (this->it != this->end && dynamic_cast<Container *>(*this->it) == nullptr) {
                ++this->it;
            }
        }

        std::vector<Element *>::iterator it;
        std::vector<Element *>::iterator end;
    };

    Iterator begin() {return {this->elements.begin(), this->elements.end()};}
    Iterator end() {return this->elements.end();}



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
