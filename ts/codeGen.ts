import {
	CodeGenText,
} from "./types";
import { BuilderContext, callFunction } from "./builder";
import { ScopeObject } from "./types";
import { getString, onCodeGen } from "./builtin";

export default {
	load(context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.name) {
			if (!context.options.compileTime && context.options.codeGenText != null && onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [getString(alias.symbolName)], "builtin", false, true, context.options.codeGenText);
			}
		}
	},
	
	function(context: BuilderContext, fn: ScopeObject, codeBlockText: CodeGenText) {
		if (fn.kind == "function" && codeBlockText) {
			if (!context.options.compileTime && context.options.codeGenText != null && onCodeGen["fn"]) {
				callFunction(context, onCodeGen["fn"], [getString(fn.symbolName), getString(codeBlockText[0])], "builtin", false, true, context.options.codeGenText);
			}
		}
	},
}