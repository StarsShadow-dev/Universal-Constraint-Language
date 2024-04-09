import {
	CodeGenText,
} from "./types";
import { BuilderContext, callFunction } from "./builder";
import { ScopeObject } from "./types";
import { getString, onCodeGen } from "./builtin";

let top: [string] = [""];

export default {
	getTop() {
		return top;	
	},
	
	start(context: BuilderContext) {
		if (onCodeGen["start"]) {
			callFunction(context, onCodeGen["start"], [], "builtin", false, true, null, top);
		}
	},
	
	load(dest: CodeGenText, context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.name) {
			if (onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [getString(alias.symbolName)], "builtin", false, true, null, dest);
			}
		}
	},
	
	function(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, codeBlockText: CodeGenText) {
		if (fn.kind == "function" && codeBlockText) {
			if (onCodeGen["fn"]) {
				debugger;
				callFunction(context, onCodeGen["fn"], [getString(fn.symbolName), getString(codeBlockText[0])], "builtin", false, true, null, dest);
			}
		}
	},
	
	call(dest: CodeGenText, context: BuilderContext, fn: ScopeObject) {
		if (dest && fn.kind == "function" && dest) {
			if (onCodeGen["call"]) {
				callFunction(context, onCodeGen["call"], [getString(fn.symbolName)], "builtin", false, true, null, dest);
			}
		}
	}
}