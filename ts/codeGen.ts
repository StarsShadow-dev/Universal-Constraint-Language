import {
	CodeGenText,
	getCGText,
	unwrapScopeObject,
} from "./types";
import { BuilderContext, callFunction } from "./builder";
import { ScopeObject } from "./types";
import { getBool, getString, onCodeGen } from "./builtin";
import utilities from "./utilities";

let top: [string] = [""];

export default {
	getTop() {
		return top;	
	},
	
	start(context: BuilderContext) {
		if (onCodeGen["start"]) {
			callFunction(context, onCodeGen["start"], [], "builtin", false, true, null, top, null);
		}
	},
	
	string(dest: CodeGenText, context: BuilderContext, text: string) {
		if (onCodeGen["string"]) {
			callFunction(context, onCodeGen["string"], [getString(text)], "builtin", false, true, null, dest, null);
		}
	},
	
	load(dest: CodeGenText, context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.name) {
			if (onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [getString(alias.symbolName)], "builtin", false, true, null, dest, null);
			}
		}
	},
	
	function(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, codeBlockText: CodeGenText) {
		if (fn.kind == "function" && codeBlockText) {
			if (onCodeGen["fn_arg"] && onCodeGen["fn"]) {
				const argDest = getCGText();
				for (let i = 0; i < fn.functionArguments.length; i++) {
					const argument = fn.functionArguments[i];
					if (argument.kind == "argument") {
						callFunction(context, onCodeGen["fn_arg"], [getString(argument.name), unwrapScopeObject(argument.type[0]), getBool(fn.functionArguments[i+1] != undefined)], "builtin", false, true, null, argDest, null);
					} else {
						utilities.unreachable();
					}
				}
				callFunction(context, onCodeGen["fn"], [getString(fn.symbolName), getString(argDest[0]), getString(codeBlockText[0])], "builtin", false, true, null, dest, null);
			}
		}
	},
	
	call(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, argumentText: CodeGenText) {
		if (dest && fn.kind == "function" && dest && argumentText) {
			if (onCodeGen["call_arg"] && onCodeGen["call"]) {
				// const argDest = getCGText();
				// for (let i = 0; i < callArguments.length; i++) {
				// 	debugger;
				// 	callFunction(context, onCodeGen["call_arg"], [unwrapScopeObject(callArguments[i]), getBool(callArguments[i+1] != undefined)], "builtin", false, true, null, argDest, null);
				// }
				callFunction(context, onCodeGen["call"], [getString(fn.symbolName), getString(argumentText[0])], "builtin", false, true, null, dest, null);
			}
		}
	}
}