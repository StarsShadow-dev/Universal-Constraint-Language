import path from "path";

import utilities from "./utilities";
import { CompilerStage, FileContext, compileFile } from "./compiler";
import { CompileError } from "./report";
import { getAlias, getNextSymbolName } from "./builder";
import { BuilderContext } from "./compiler";
import {
	ASTnode,
	ScopeObject,
	ScopeObjectType,
	ScopeObjectType_getId,
	ScopeObject_alias,
	ScopeObject_complexValue,
	ScopeObject_function,
	ScopeObject_struct,
	cast_ScopeObjectType,
	is_ScopeObjectType,
	unwrapScopeObject,
} from "./types";

let fileSystemDisabled = false;

export let builtinScopeLevel: ScopeObject_alias[] = [];

export let onCodeGen: any = {};

const typeType: ScopeObject_struct = {
	kind: "struct",
	originLocation: "builtin",
	id: "builtin:Type",
	preIdType: null,
	toBeChecked: false,
	fields: [],
	members: [],
};

function addType(name: string) {
	builtinScopeLevel.push({
		kind: "alias",
		originLocation: "builtin",
		isAfield: false,
		name: name,
		symbolName: "",
		value: {
			kind: "struct",
			originLocation: "builtin",
			id: "builtin:" + name,
			preIdType: null,
			toBeChecked: false,
			fields: [],
			members: [],
		},
		valueAST: null,
	});
}

export function getBool(value: boolean): ScopeObject {
	return {
		kind: "bool",
		originLocation: "builtin",
		value: value,
	};
}

export function getNumber(value: number): ScopeObject {
	return {
		kind: "number",
		originLocation: "builtin",
		value: value,
	};
}

export function getString(value: string): ScopeObject {
	return {
		kind: "string",
		originLocation: "builtin",
		value: value,
	};
}

export function getInAlias(name: string, value: ScopeObject): ScopeObject_alias {
	return {
		kind: "alias",
		originLocation: "builtin",
		isAfield: false,
		name: name,
		symbolName: "",
		value: value,
		valueAST: null,
	};
}

export function getStruct(context: BuilderContext, members: ScopeObject_alias[], toBeChecked: boolean, id?: string): ScopeObject_struct {
	if (!id) {
		id = `${getNextSymbolName(context)}`
	}
	return {
		kind: "struct",
		originLocation: "builtin",
		id: id,
		preIdType: null,
		toBeChecked: toBeChecked,
		fields: [],
		members: members,
	};
}

export function getComplexValueFromString(context: BuilderContext, name: string): ScopeObject_complexValue {
	return {
		kind: "complexValue",
		originLocation: "builtin",
		type: cast_ScopeObjectType(getAlias(context, name)),
	};
}

export function setUpBuiltin(disableFileSystem: boolean) {
	fileSystemDisabled = disableFileSystem;
	if (builtinScopeLevel.length == 0) {
		builtinScopeLevel.push({
			kind: "alias",
			originLocation: "builtin",
			isAfield: false,
			name: "Type",
			symbolName: "",
			value: typeType,
			valueAST: null,
		});
		addType("Bool");
		addType("Number");
		addType("String");
		addType("Any");
		addType("__UnknownType");
	}
	
	if (Object.keys(onCodeGen).length != 0) {
		onCodeGen = {};
	}
}

// builtin function constructor
class FC {
	private builtinNode: ASTnode
	private scopeObjects: ScopeObject[]
	private nodes: ASTnode[]
	private i: number
	
	constructor(builtinNode: ASTnode, scopeObjects: ScopeObject[], nodes: ASTnode[]) {
		this.builtinNode = builtinNode;
		this.scopeObjects = scopeObjects;
		this.nodes = nodes;
		this.i = 0;
	}
	
	public get(name: string, comptime: boolean, unwrap?: boolean): ScopeObject {
		if (unwrap == undefined) unwrap = true;
		
		let scopeObject = this.scopeObjects[this.i];
		const node = this.nodes[this.i];
		
		if (!scopeObject || !node) {
			throw new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, `expected ${name} but there are no more arguments`);
		}
		
		this.i++;
		
		if (unwrap) scopeObject = unwrapScopeObject(scopeObject);
		
		if (scopeObject.kind == "complexValue") {
			if (comptime) {
				throw new CompileError(`builtin expected comptime '${name}' but got not comptime '${name}'`)
					.indicator(node.location, `here`);
			}
			
			throw utilities.TODO();
		} else {
			if (scopeObject.kind != name) {
				throw new CompileError("builtin argument error")
					.indicator(node.location, `expected ${name}`);
			}
			
			return scopeObject;
		}
	}
	
	public "bool" = (comptime: boolean): boolean => {
		const value = this.get("bool", comptime);
		if (value.kind == "bool") {
			return value.value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public "string" = (comptime: boolean): string => {
		const value = this.get("string", comptime);
		if (value.kind == "string") {
			return value.value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public "function" = (comptime: boolean): ScopeObject_function => {
		const value = this.get("function", comptime);
		if (value.kind == "function") {
			return value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public "alias" = (comptime: boolean): ScopeObject_alias => {
		const value = this.get("alias", comptime, false);
		if (value.kind == "alias") {
			return value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public type(): ScopeObjectType {
		const type = this.forward();
		
		if (!is_ScopeObjectType(type)) {
			throw utilities.TODO();
		}
		
		return type;
	}
	
	public next(): ScopeObject {
		const scopeObject = this.scopeObjects[this.i];
		return scopeObject;
	}
	
	public forward(): ScopeObject {
		const scopeObject = this.scopeObjects[this.i];
		this.i++;
		return scopeObject;
	}
	
	public done() {
		const node = this.nodes[this.i];
		
		if (node) {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected no more arguments");
		}
	}
}

export function type_List(context: BuilderContext, T: ScopeObjectType): ScopeObjectType {
	return getStruct(context, [
		getInAlias("T", T),
	], false, `List(${ScopeObjectType_getId(T)})`);
}

export function builtinCall(context: BuilderContext, node: ASTnode, callArguments: ScopeObject[], argumentText: string[]): ScopeObject | undefined {
	if (node.kind == "builtinCall") {
		const fc = new FC(node, callArguments, node.callArguments);
		
		if (node.name == "compDebug") {
			console.log("compDebug", callArguments);
			debugger;
		}
		
		else if (node.name == "compAssert") {
			const bool = fc.bool(true);
			
			if (!bool) {
				throw new CompileError(`assert failed`)
					.indicator(node.location, "here");
			}
		}
		
		else if (node.name == "import") {
			const filePath = fc.string(true);
			fc.done();
			
			const oldCompilerStage = context.compilerStage;
			context.compilerStage = CompilerStage.findAliases;
			
			let newContext: FileContext;
			try {
				newContext = compileFile(context, path.join(path.dirname(context.file.path), filePath), null);
			} catch (error) {
				if (error instanceof CompileError) {
					context.errors.push(error);
					throw "__done__";
				} else {
					throw error;
				}
			}
			
			context.compilerStage = oldCompilerStage;
			
			return getStruct(context, newContext.scope.levels[0] as ScopeObject_alias[], false);
		}
		
		else if (node.name == "List") {
			const type = fc.type();
			fc.done();
			
			return type_List(context, type);
		}
		
		else {
			throw new CompileError(`no builtin named ${node.name}`).indicator(node.location, "here");
		}
	} else {
		utilities.unreachable();
	}
}