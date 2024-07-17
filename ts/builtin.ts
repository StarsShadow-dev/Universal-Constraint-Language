import { ASTnode_alias, ASTnodeType } from "./parser";
import utilities from "./utilities";

function makeBuiltinType(name: string): (ASTnode_alias & { value: {kind: "struct"} }) {
	return {
		kind: "alias",
		location: "builtin",
		name: name,
		value: {
			kind: "struct",
			location: "builtin",
			id:  "builtin:" + name,
			fields: [],
		},
	};
}

export const builtinTypes: (ASTnode_alias & { value: {kind: "struct"} })[] = [
	makeBuiltinType("Type"),
	makeBuiltinType("Bool"),
	makeBuiltinType("Number"),
	makeBuiltinType("String"),
];

export function getBuiltinType(name: string): ASTnodeType {
	for (let i = 0; i < builtinTypes.length; i++) {
		const alias = builtinTypes[i];
		if (alias.name == name) {
			return alias.value;
		}
	}
	
	throw utilities.unreachable();
}