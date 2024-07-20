import utilities from "./utilities";
import { BuilderContext } from "./db";
import { evaluate } from "./evaluate";
import { CompileError } from "./report";
import {
	ASTnode,
	ASTnodeType,
	ASTnode_alias,
	ASTnode_builtinCall,
} from "./parser";

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
	makeBuiltinType("Effect"),
	makeBuiltinType("Function"),
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

export function isBuiltinType(type: ASTnodeType, name: string): boolean {
	return type.id.split(":")[1] == name;
}

export function evaluateBuiltin(context: BuilderContext, builtinCall: ASTnode_builtinCall): ASTnode {
	let index = 0;
	function getArg(): ASTnode {
		const node = builtinCall.callArguments[index];
		if (node == undefined) {
			throw new CompileError(`not enough arguments for builtin`)
				.indicator(builtinCall.location, "here");
		}
		index++;
		return evaluate(context, node);
	}
	function moreArgs(): boolean {
		return index < builtinCall.callArguments.length;
	}
	function getBool(): boolean {
		const opCode = getArg();
		if (opCode.kind != "bool") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	function getString(): string {
		const opCode = getArg();
		if (opCode.kind != "string") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	function getNumber(): number {
		const opCode = getArg();
		if (opCode.kind != "number") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	
	if (builtinCall.name == "Number") {
		const min = getNumber();
		const max = getNumber();
		return {
			kind: "struct",
			location: builtinCall.location,
			id:  JSON.stringify(builtinCall.location),
			fields: [],
			data: {
				kind: "number",
				min: min,
				max: max,
			}
		};
	} else {
		throw utilities.unreachable();
	}
}