import * as utilities from "./utilities";
import { getType, BuilderContext } from "./db";
import { evaluateList } from "./evaluate";
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
		unalias: false,
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
	makeBuiltinType("Any"),
];

export function getBuiltinType(name: string): ASTnodeType {
	for (let i = 0; i < builtinTypes.length; i++) {
		const alias = builtinTypes[i];
		if (alias.name == name) {
			return alias.value;
		}
	}
	
	utilities.unreachable();
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
		return evaluateList(context, [node])[0];
	}
	function moreArgs(): boolean {
		return index < builtinCall.callArguments.length;
	}
	function getBool(): boolean {
		const opCode = getArg();
		if (opCode.kind != "bool") {
			utilities.TODO();
		}
		return opCode.value;
	}
	function getString(): string {
		const opCode = getArg();
		if (opCode.kind != "string") {
			utilities.TODO();
		}
		return opCode.value;
	}
	function getNumber(): number {
		const opCode = getArg();
		if (opCode.kind != "number") {
			utilities.TODO();
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
			},
		};
	} else if (builtinCall.name == "getError") {
		const node = builtinCall.callArguments[index];
		if (node == undefined) {
			throw new CompileError(`not enough arguments for builtin`)
				.indicator(builtinCall.location, "here");
		}
		index++;
		const input = getType(context, node);
		
		let output = "";
		if (input.kind == "error") {
			if (input.compileError) {
				output = input.compileError.msg;
			} else {
				utilities.unreachable();
			}
		}
		
		return {
			kind: "string",
			location: builtinCall.location,
			value: output,
		};
	} else {
		utilities.unreachable();
	}
}