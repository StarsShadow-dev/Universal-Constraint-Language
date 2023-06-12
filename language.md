# Parallel Language Design

**Note, that not all of these features are finished/implemented.**

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
external function putchar(int: Int32): Int32 "
	declare i32 @putchar(i32 noundef) #1
"
```

## Types

```
Void

Int8
Int32
Int64
```

## Attributes

The attribute syntax is based on [rust](https://doc.rust-lang.org/rust-by-example/attribute.html).

```
#[attribute]
```