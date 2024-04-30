import * as fs from "fs";
import { stdout } from "process";
import path from "path";

import { compileFile } from "./compiler";
import {
	ASTnode,
	ScopeObject,
	getCGText,
	unwrapScopeObject,
} from "./types";
import { CompileError } from "./report";
import utilities from "./utilities";
import { BuilderContext, callFunction, getAlias, getAsComptimeType, getNextSymbolName, getTypeOf, getTypeText } from "./builder";
import codeGen from "./codeGen";

let started = false;

let fileSystemDisabled = false;

export let builtinScopeLevel: ScopeObject[] = [];

export let onCodeGen: any = {};

const typeType: ScopeObject & { kind: "typeUse" } = {
	kind: "typeUse",
	originLocation: "builtin",
	comptime: true,
	type: {
		kind: "struct",
		originLocation: "builtin",
		name: "builtin:Type",
		templateStruct: null,
		properties: [],
	},
};

function addType(name: string) {
	builtinScopeLevel.push({
		kind: "alias",
		originLocation: "builtin",
		mutable: false,
		isAproperty: false,
		name: name,
		value: {
			kind: "typeUse",
			originLocation: "builtin",
			comptime: false,
			type: {
				kind: "struct",
				originLocation: "builtin",
				name: "builtin:" + name,
				templateStruct: null,
				properties: [],
			},
		},
		symbolName: "",
		type: typeType,
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

export function getComplexValue(context: BuilderContext, name: string): ScopeObject & { kind: "complexValue" } {
	return {
		kind: "complexValue",
		originLocation: "builtin",
		type: getAsComptimeType(unwrapScopeObject(getAlias(context, name))),
	};
}

function getStruct(context: BuilderContext, properties: ScopeObject[]): ScopeObject {
	return {
		kind: "typeUse",
		originLocation: "builtin",
		comptime: true,
		type: {
			kind: "struct",
			originLocation: "builtin",
			name: `${getNextSymbolName(context)}`,
			templateStruct: null,
			properties: properties,
		},
	};
}

export function setUpBuiltin(disableFileSystem: boolean) {
	started = false;
	fileSystemDisabled = disableFileSystem;
	if (builtinScopeLevel.length == 0) {
		builtinScopeLevel.push({
			kind: "alias",
			originLocation: "builtin",
			mutable: false,
			isAproperty: false,
			name: "Type",
			value: typeType,
			symbolName: "",
			type: typeType,
		});
		addType("Bool");
		addType("Number");
		addType("String");
		addType("Any");
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
		
		if (comptime && scopeObject.kind == "alias") {
			if (scopeObject.type && scopeObject.type.kind == "typeUse") {
				if (!scopeObject.type.comptime) {
					throw new CompileError("builtin argument error")
						.indicator(node.location, `expected a comptime ${name}, but it is not comptime.`);
				}
			} else {
				utilities.unreachable();
			}
		}
		
		if (unwrap) scopeObject = unwrapScopeObject(scopeObject);
		
		if (scopeObject.kind == name) {
			return scopeObject;
		} else {
			throw new CompileError("builtin argument error")
				.indicator(node.location, `expected ${name}`);
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
	
	public "function" = (comptime: boolean): ScopeObject & { kind: "function" } => {
		const value = this.get("function", comptime);
		if (value.kind == "function") {
			return value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public "type" = (): ScopeObject & { kind: "typeUse" } => {
		const value = this.get("type", true, true);
		if (value.kind == "typeUse") {
			return value;
		} else {
			throw utilities.unreachable();
		}
	}
	
	public "alias" = (comptime: boolean): ScopeObject & { kind: "alias" } => {
		const value = this.get("alias", comptime, false);
		if (value.kind == "alias") {
			return value;
		} else {
			throw utilities.unreachable();
		}
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

export function builtinCall(context: BuilderContext, node: ASTnode, callArguments: ScopeObject[], argumentText: string[]): ScopeObject | undefined {
	if (node.kind == "builtinCall") {
		const fc = new FC(node, callArguments, node.callArguments);
		
		if (node.name == "compileLog") {
			let str = "[compileLog] ";
			while (fc.next()) {
				str += fc.string(true);
			}
			str += "\n"
			fc.done();
			
			stdout.write(str);
		}
		
		else if (node.name == "compileDebug") {
			console.log(callArguments);
			debugger;
		}
		
		else if (node.name == "assert") {
			const bool = fc.bool(false);
			
			if (!bool) {
				throw new CompileError(`assert failed`)
					.indicator(node.location, "here");
			}
		}
		
		else if (node.name == "opaque") {
			const value = fc.forward();
			fc.done();
			
			if (context.options.codeGenText) {
				context.options.codeGenText.push(argumentText.join(""));
			}
			
			return {
				kind: "complexValue",
				originLocation: "builtin",
				type: getTypeOf(context, value),
			};
		}
		
		else if (node.name == "addCodeGen") {
			let str = "";
			while (fc.next()) {
				str += fc.string(true);
			}
			fc.done();
			
			if (context.options.codeGenText) {
				context.options.codeGenText.push(str);
			} else {
				console.log("no context.options.codeGenText", str);
			}
		}
		
		else if (node.name == "addTopCodeGen") {
			let str = "";
			while (fc.next()) {
				str += fc.string(true);
			}
			fc.done();
			
			context.topCodeGenText.push(str);
		}
		
		else if (node.name == "onCodeGen") {
			const name = fc.string(true);
			const fn = fc.function(true);
			fc.done();
			
			onCodeGen[name] = fn;
		}
		
		else if (node.name == "writeToStdout") {
			fc.done();
			
			console.log(context.topCodeGenText.join(""));
		}
		
		else if (node.name == "writeTofile") {
			if (fileSystemDisabled) {
				utilities.unreachable();
			}
			
			const outPath = fc.string(true);
			fc.done();
			
			const newPath = path.join(path.dirname(context.filePath), outPath);
			const dir = path.dirname(newPath);
			
			if (!fs.existsSync(dir)) {
				fs.mkdirSync(dir, { recursive: true });
			}
			
			fs.writeFileSync(newPath, context.topCodeGenText.join(""));
		}
		
		else if (node.name == "import") {
			const filePath = fc.string(true);
			fc.done();
			
			const newContext = compileFile(path.join(path.dirname(context.filePath), filePath));
			
			return getStruct(context, newContext.scope.levels[0]);
		}
		
		else if (node.name == "export") {
			const name = fc.string(true);
			const fnAlias = fc.alias(false);
			const fn = unwrapScopeObject(fnAlias);
			fc.done();
			
			if (fn.kind == "function") {
				fn.symbolName = name;
			}
			
			if (!started) {
				codeGen.start(context);
				started = true;
			}
			
			callFunction(context, fn, null, node.location, false, null, null, null);
		}
		
		else {
			throw new CompileError(`no builtin named ${node.name}`).indicator(node.location, "here");
		}
	} else {
		utilities.unreachable();
	}
}