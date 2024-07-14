import path from "path";
import { BuilderContext, FileContext, compileFile } from "./compiler";
import { CompileError } from "./report";
import { OpCode, OpCodeType, OpCode_alias, OpCode_builtinCall, OpCode_isAtype } from "./types";
import utilities from "./utilities";
import { build, getAliasFromList } from "./builder";

function makePrimitiveTypeAlias(name: string): OpCode_alias {
	return {
		kind: "alias",
		location: "builtin",
		disableGeneration: true,
		name: name,
		value: {
			kind: "struct",
			location: "builtin",
			id: `builtin:${name}`,
			haveSetId: false,
			caseTag: null,
			doCheck: false,
			fields: [],
			codeBlock: [],
		},
	}
}

export const builtinTypes: OpCode_alias[] = [
	makePrimitiveTypeAlias("Type"),
	makePrimitiveTypeAlias("Bool"),
	makePrimitiveTypeAlias("Number"),
	makePrimitiveTypeAlias("String"),
];

function castAliasToType(alias: OpCode_alias | null): OpCodeType {
	if (!alias || !OpCode_isAtype(alias.value)) {
		throw utilities.unreachable();
	}
	
	return alias.value;
}

export function getBuiltinType(name: string): OpCodeType {
	return castAliasToType(getAliasFromList(builtinTypes, name));
}

export function builtinCall(context: BuilderContext, callOpCode: OpCode_builtinCall, resolve: boolean): OpCode {
	let index = 0;
	function getArg(resolve: boolean): OpCode {
		const opCode = callOpCode.callArguments[index];
		if (opCode == undefined) {
			throw new CompileError(`not enough arguments for builtin`)
				.indicator(callOpCode.location, "here");
		}
		index++;
		return build(context, opCode, resolve);
	}
	function moreArgs(): boolean {
		return index < callOpCode.callArguments.length;
	}
	function getBool(): boolean {
		const opCode = getArg(true);
		if (opCode.kind != "bool") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	function getString(): string {
		const opCode = getArg(true);
		if (opCode.kind != "string") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	
	if (callOpCode.name == "compAssert") {
		debugger;
		const ok = getBool();
		if (!ok) {
			throw new CompileError(`assert failed`)
				.indicator(callOpCode.location, "here");
		}
	} else if (callOpCode.name == "db") {
		let argumentList = [];
		while (moreArgs()) {
			argumentList.push(getArg(true));
		}
		debugger;
	} else if (callOpCode.name == "import") {
		if (context.disableImports) {
			return callOpCode;
		}
		
		const pathInput = getString();
		const filePath = path.join(path.dirname(context.file.path), pathInput);
		
		let newContext: FileContext;
		try {
			newContext = compileFile(context, filePath, null);
		} catch (error) {
			if (error instanceof CompileError) {
				context.errors.push(error);
				throw "__done__";
			} else {
				throw error;
			}
		}
		
		return {
			kind: "struct",
			location: callOpCode.location,
			id: `${filePath}`,
			haveSetId: false,
			caseTag: null,
			doCheck: false,
			fields: [],
			codeBlock: newContext.opCodes,
		};
	} else {
		throw new CompileError(`no builtin named '${callOpCode.name}'`)
			.indicator(callOpCode.location, "here");
	}
	
	return callOpCode;
}