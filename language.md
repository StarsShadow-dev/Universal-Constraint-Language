# Parallel Language Design

**Note, that not all of these features are implemented.**

## Functions

main function:
```
function main(): Int32 {
	return 0;
}
```

external function:
```
// when the external keyword is used before a function the contents of the function is a string that is placed into the LLVM input
external function putchar(char: Int32): Int32 "
	declare i32 @putchar(i32 noundef) #1
"
```

## Variables

the Syntax for defining a variable:

```
var myVariable: Int32 = 5;
```

## Types

```
Void

Int8
Int32
Int64
```

## Attributes

The attribute syntax is inspired by [rust](https://doc.rust-lang.org/rust-by-example/attribute.html).

```
#[attribute]
```