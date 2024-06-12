import { OpCode_alias } from "./types";

function makePrimitiveTypeAlias(name: string): OpCode_alias {
	return {
		kind: "alias",
		location: "builtin",
		name: name,
		value: {
			kind: "struct",
			location: "builtin",
			id: `builtin:${name}`,
			fields: [],
			codeBlock: [],
		},
	}
}

export const builtinTypes: OpCode_alias[] = [
	makePrimitiveTypeAlias("Bool"),
	makePrimitiveTypeAlias("Number"),
	makePrimitiveTypeAlias("String"),
];