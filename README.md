# Configuration Parser
This is a comprehensive stand-alone configuration file parser that is intended to be used for runtime configuration in a compiler. It is designed to pull configuration information from one or more files, from the environment, and then from the command line, in that order. When a duplicate configuration item is encountered, then the value is replaced, appended, or pre-pended according to the api that stores it.

- Configuration items are drawn from one or more files, the environment, and the command line, in that order.
- Including additional configuration files is supported.
- Conditional selection of text is supported.
- Duplicate items that are found can be overwritten, appended, or pre-pended according to the API used.
- Configuration is **not** intended to be edited by the compiler user, but it is intended to be present when the system is used.
- Command line configuration for a given app is nested into the file, along with acceptable types.
- Environment variables to search for are given in the configuration.
- The configuration is stand alone and supports multiple applications.
- Configuration sections can be nested to any depth.
- Value substitutions are implemented.
- Including files into the configuration is supported.

## File Format
The general format of a configuration file is a section name followed by a block that is enclosed in '{}' characters. A block contains name/value pairs that are separated by a '=' character. Values in blocks are given a composite name that consists of all of the parent blocks. All name/value pairs must be members of a block. For example, the value of ```foo{bar{name=value}}``` is ```foo.bar.name=value```. The value ```bacon{eggs{name=value}}``` names a different value instance. If a curly brace or an equal sign are desired in a value they can be escaped with a backslash. If a value needs to contain a backslash, it can be escaped in the usual manner.

Comments are formatted with a '#' at the start of the comment and ends with the newline. A value can contain a '#' by escaping it with a backslash.

No name can contain ```[{}\#=:".$()]``` or any keyword. Values can contain these characters by escaping them with a backslash.

- Types of variable supported
    - Integer. Has the format of ```[0-9]*``` and is converted as (int)strtol(str, NULL, 10).
    - Hex number. Has the format of ```0x[0-9a-fA-F]*``` and is converted as (unsigned)strtol(str, NULL, 16). These are stored as integers and the hex notion is for convenience only.
    - Float. Any string that can be converted with strtod(). Scanned when there is a '.' in the number.
    - Quoted String. A series of characters, including white space where C style escape characters are converted to their binary equivalents, including 'x??' and 'x????'. Quoted string can span lines without escapes but newlines are deleted from the string and must be inserted where desired explicitly.
    - String. And ASCII string that does not contain white space.  Extra white space is a syntax error.
    - Boolean. Can be true, false, on, off, 1, or 0. Not case-sensitive.

- All values are considered to be lists, even if there is just one item in it. List items are separated by a ':' character. If a value must contain a ':', then it must be escaped with a backslash. Lists can contain any number of items and each item can be of any type of value.

- Value substitutions are given as surrounded by ```$()``` and are specified as the fully qualified name. Substitutions can be embedded in strings or any other value as text only. Substitutions are automatically made when the value is read such that if it was changed after it was scanned and parsed, then the new value is given. Substitutions may be used in include statements, but not as names.

## Keywords
A keyword is a reserved word that cannot be used as a name or as a value. These words are not case-sensitive.
- include. Must appear outside of all sections.
- cmdline. For defining command line options.
