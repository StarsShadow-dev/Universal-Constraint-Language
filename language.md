# Parallel Language Design

**Note, that not all of these features are currently implemented.**

## Functions

* A function can be called from within itself.

main function:
```parallel
function main(): Int32 {
	return 0;
}
```

external function:
```parallel
// when there is a string after the return type this is an external function and the string is placed into the LLVM input
function putchar(char: Int32): Int32 "
	declare i32 @putchar(i32 noundef) #1
"
```

## Variables

the Syntax for defining a variable:

```parallel
var myVariable: Int32 = 5;
```

## Types

The names of the integers are literally copied from Swift.

```parallel
Void

Int8
Int16
Int32
Int64

UInt8
UInt16
UInt32
UInt64

Bool
Pointer
```

