import * as fs from "fs";
import { stdout } from "process";
import path from "path";

import { compileFile } from "./compiler";
import {
	ASTnode,
	ScopeObject,
} from "./types";
import { CompileError } from "./report";
import utilities from "./utilities";
import { BuilderContext, callFunction } from "./builder";

let fileSystemDisabled = false;

export let builtinScopeLevel: ScopeObject[] = [];

let onCodeGen: any = {};

function addType(name: string) {
	builtinScopeLevel.push({
		kind: "alias",
		mutable: false,
		originLocation: "builtin",
		name: name,
		value: {
			kind: "type",
			originLocation: "builtin",
			name: "builtin:" + name
		},
	});
}

function getString(value: string): ScopeObject {
	return {
		kind: "string",
		originLocation: "builtin",
		value: value,
	};
}

function getStruct(properties: ScopeObject[]): ScopeObject {
	return {
		kind: "struct",
		originLocation: "builtin",
		properties: properties,
	};
}

export function setUpBuiltin(disableFileSystem: boolean) {
	fileSystemDisabled = disableFileSystem;
	if (builtinScopeLevel.length == 0) {
		addType("Bool");
		addType("Number");
		addType("String");	
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
	
	public "string" = (): string => {
		const scopeObject = this.scopeObjects[this.i];
		const node = this.nodes[this.i];
		
		if (!scopeObject || !node) {
			throw new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, "expected a string but there are no more arguments");
		}
		
		this.i++;
		
		if (scopeObject.kind == "string") {
			return scopeObject.value;
		} else {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected a string");
		}
	}
	
	public "function" = (): ScopeObject => {
		const scopeObject = this.scopeObjects[this.i];
		const node = this.nodes[this.i];
		
		if (!scopeObject || !node) {
			throw new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, "expected a function but there are no more arguments");
		}
		
		this.i++;
		
		if (scopeObject.kind == "function") {
			return scopeObject;
		} else {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected a function");
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

export function builtinCall(context: BuilderContext, node: ASTnode, callArguments: ScopeObject[]): ScopeObject | undefined {
	if (node.kind == "builtinCall") {
		const fc = new FC(node, callArguments, node.callArguments);
		
		if (node.name == "compileLog") {
			let str = "[compileLog] ";
			while (fc.next()) {
				str += fc.string();
			}
			str += "\n"
			fc.done();
			
			stdout.write(str);
		}
		
		else if (node.name == "addCodeGen") {
			let name = fc.string();
			
			let str = "";
			while (fc.next()) {
				str += fc.string();
			}
			fc.done();
			
			if (context.codeGenText[name] == undefined) {
				context.codeGenText[name] = "";
			}
			
			context.codeGenText[name] += str;
		}
		
		else if (node.name == "moveCodeGen") {
			let destName = fc.string();
			let srcName = fc.string();
			fc.done();
			
			if (context.codeGenText[destName] == undefined) {
				context.codeGenText[destName] = "";
			}
			
			if (context.codeGenText[srcName]) {
				context.codeGenText[destName] += context.codeGenText[srcName];
			}
			
			context.codeGenText[srcName] = "";
		}
		
		else if (node.name == "onCodeGen") {
			const name = fc.string();
			const fn = fc.function();
			fc.done();
			
			onCodeGen[name] = fn;
		}
		
		else if (node.name == "writeToStdout") {
			const name = fc.string();
			fc.done();
			
			stdout.write(context.codeGenText[name]);
			
			context.codeGenText[name] = "";
		}
		
		else if (node.name == "writeTofile") {
			if (fileSystemDisabled) {
				utilities.unreachable();
			}
			
			const name = fc.string();
			const outPath = fc.string();
			fc.done();
			
			if (typeof context.codeGenText[name] == "string") {
				const newPath = path.join(path.dirname(context.filePath), outPath);
				const dir = path.dirname(newPath);
				
				console.log(`writing to '${outPath}':\n${context.codeGenText[name]}`);
				
				if (!fs.existsSync(dir)) {
					fs.mkdirSync(dir, { recursive: true });
				}
				
				fs.writeFileSync(newPath, context.codeGenText[name]);
				context.codeGenText[name] = "";
			} else {
				throw new CompileError(`unable to write '${name}' to file '${outPath}' because '${name}' does not exist`).indicator(node.location, "here");
			}
		}
		
		else if (node.name == "import") {
			const filePath = fc.string();
			fc.done();
			
			const scopeLevels = compileFile(path.join(path.dirname(context.filePath), filePath));
			
			return getStruct(scopeLevels[0]);
		}
		
		else if (node.name == "export") {
			const name = fc.string();
			const fn = fc.function();
			fc.done();
			
			if (onCodeGen["fn"]) {
				context.codeGenText["__fn_name"] = name;
				
				callFunction(context, fn, [], "builtin", true);
				callFunction(context, onCodeGen["fn"], [getString(name)], "builtin", false);
			}
		}
		
		else {
			throw new CompileError(`no builtin named ${node.name}`).indicator(node.location, "here");
		}
	} else {
		utilities.unreachable();
	}
	
	return;
}