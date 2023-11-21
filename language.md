# Parallel Language Design

**Note, that not all of these features are currently implemented/working.**

## Functions

* A function can be called from within itself.

main function:
```parallel
function main(): Int32 {
	return 0;
}
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

Add "Never"?

## macros

Macro definition:

```parallel
macro nameOfMacro {
	// ...
}
```

Macro use:

```parallel
#nameOfMacro();
```

## constraints

Syntax idea:

Should `$` be self?

<!-- ```parallel
typedef Degrees: Float [ $ >= -360, $ <= 360 ]
``` -->


```parallel
function getRand0_100(): Int8 [ $ >= 0, $ <= 100 ] {
	// ...
}
```

```parallel
typedef Window: Pointer;
var window: Window;

function [ window != 0 ] useWindow(): Void {}
// or
function useWindow() [ window != 0 ]: Void {}
```