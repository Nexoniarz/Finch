#pragma once
// One description of every built-in function and method, shared by the language server
// (hover, completion, signature help).

struct BuiltinDoc {
    const char *name;
    const char *signature;
    const char *doc;
};

inline const BuiltinDoc kBuiltins[] = {
    {"print", "print(a, b, ...)", "Prints the values separated by spaces, then a new line. Works with every type."},
    {"input", "input(str question = \"\") -> str", "Shows the question and reads one line typed by the user, without the newline."},
    {"read_file", "read_file(str path) -> str", "The whole file as text. Stops the program if it can't be read."},
    {"write_file", "write_file(str path, str text) -> bool", "Writes (replaces) the file. true if it worked."},
    {"file_exists", "file_exists(str path) -> bool", "Is there a file at this path?"},
    {"delete_file", "delete_file(str path) -> bool", "Deletes the file. true if it worked."},
    {"shell", "shell(str command) -> int", "Runs a command in the system's shell and gives back its exit code."},
    {"exit", "exit(int code)", "Ends the program right away with this exit code."},
    {"addr", "addr(x) -> ptr[T]", "A pointer to a variable, field or element. addr(function) gives C a callback."},
    {"new", "new(value) -> ptr[T]", "Puts a copy of the value on the heap. Give it back with free(...)."},
    {"free", "free(ptr p)", "Gives back memory from new(...) (or from C), freeing what the value owns."},
    {"str", "str(x) -> str", "Turns a number, bool, char, []u8 or C char pointer into text."},
    {"int", "int(x) -> int", "Converts to a whole number: cuts off fractions, parses text (stops the program if it isn't a number)."},
    {"float", "float(x) -> float", "Converts to a decimal number; parses text."},
    {"char", "char(int n) -> char", "The character with this code."},
    {"bool", "bool(int n) -> bool", "true for any number but 0."},
    {"ptr", "ptr(x) -> ptr", "An untyped pointer: from a typed pointer, or from a number (an address or offset for C)."},
};

inline const BuiltinDoc kArrayMethods[] = {
    {"len", ".len -> int", "The number of elements."},
    {"ptr", ".ptr -> ptr[T]", "The address of the first element, for C."},
    {"push", "push(T x)", "Adds x at the end."},
    {"pop", "pop() -> T", "Removes the last element and gives it back."},
    {"insert", "insert(int i, T x)", "Puts x at position i, moving the rest."},
    {"remove", "remove(int i) -> T", "Removes position i and gives it back."},
    {"clear", "clear()", "Removes everything."},
    {"resize", "resize(int n)", "Makes the array n long; new elements are zero."},
    {"find", "find(T x) -> int", "The index of x, or -1."},
    {"contains", "contains(T x) -> bool", "Is x in the array?"},
    {"sort", "sort()", "Sorts in place (numbers, chars, str)."},
    {"reverse", "reverse()", "Reverses in place."},
    {"slice", "slice(int start, int end) -> []T", "A new array with the elements start to end-1."},
    {"join", "join(str sep) -> str", "For []str: one text with sep between the parts."},
};

inline const BuiltinDoc kStrMethods[] = {
    {"len", ".len -> int", "The number of bytes."},
    {"ptr", ".ptr -> ptr[char]", "The address of the text, for C (valid while the str lives)."},
    {"sub", "sub(int start, int end) -> str", "The part from start to end-1."},
    {"find", "find(str t) -> int", "Where t starts, or -1."},
    {"contains", "contains(str t) -> bool", "Does the text contain t?"},
    {"starts_with", "starts_with(str t) -> bool", "Does the text start with t?"},
    {"ends_with", "ends_with(str t) -> bool", "Does the text end with t?"},
    {"split", "split(str sep) -> []str", "The parts between each sep; split(\"\") gives single characters."},
    {"trim", "trim() -> str", "Without spaces and newlines at both ends."},
    {"upper", "upper() -> str", "With a-z changed to A-Z."},
    {"lower", "lower() -> str", "With A-Z changed to a-z."},
    {"replace", "replace(str from, str to) -> str", "Every from replaced by to."},
    {"repeat", "repeat(int n) -> str", "The text n times."},
    {"bytes", "bytes() -> []u8", "The bytes of the text."},
};

inline const char *kKeywords[] = {"fn", "return", "if", "else", "while", "for", "in", "break", "continue", "true",
                                  "false", "null", "import", "link", "struct", "defer"};

inline const char *kTypeNames[] = {"int", "float", "bool", "char", "str", "ptr", "i8", "i16", "i32", "i64",
                                   "u8", "u16", "u32", "u64", "f32", "f64"};
