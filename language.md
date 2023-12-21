# Language Design

**Note, that not all of these features are currently implemented/working.**

## Functions

* A function can be called from within itself.

main function:
```llcl
function main(): Int32 {
	return 0;
}
```

## Variables

the Syntax for defining a variable:

```llcl
var myVariable: Int32 = 5;
```

## Operators

Listed from lowest precedence to highest precedence.

```
assignment =

or ||

and &&

equivalent ==
not equivalent !=
greater than >
less than <

add +
subtract -

multiply *
divide /
modulo %

cast as

member access .

scope resolution ::
```

## Types

The names of the integers are literally copied from Swift.

```llcl
Void

Int8
Int16
Int32
Int64

UInt8
UInt16
UInt32
UInt64

Float16
Float32
Float64

Bool
Pointer
```

## macros

Macro definition:

```llcl
macro nameOfMacro {
	// ...
}
```

Macro use:

```llcl
#nameOfMacro();
```

## constraints

Syntax idea:

Should `$` be self?

<!-- ```llcl
typedef Degrees: Float [ $ >= -360, $ <= 360 ]
``` -->


```llcl
function getRand0_100(): Int8 [ $ >= 0, $ <= 100 ] {
	// ...
}
```

```llcl
typedef Window: Pointer;
var window: Window;

function [ window != 0 ] useWindow(): Void {}
// or
function useWindow() [ window != 0 ]: Void {}
```