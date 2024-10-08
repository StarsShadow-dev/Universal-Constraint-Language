import crypto from "crypto";

import * as utilities from "./utilities.js";
import logger, { LogType } from "./logger.js";
import { CompileError, Indicator } from "./report.js";
import { builtins, builtinTypes } from "./builtin.js";
import {
	ASTnode,
	ASTnode_alias,
	ASTnode_command,
	ASTnode_error,
	ASTnode_identifier,
	ASTnodeType,
	BuilderContext,
	CodeGenContext,
} from "./ASTnodes.js";

type Hash = string;
function hashString(text: string): Hash {
	return crypto.createHash("sha256").update(text).digest("hex");
}
function getUUID(): string {
	const byteSize = 10;
	const bytes = crypto.getRandomValues(new Uint8Array(byteSize));
	let string = "";
	for (let i = 0; i < bytes.length; i++) {
		const byteString = bytes[i].toString(16).toUpperCase();
		if (byteString.length == 1) {
			string += "0";
		}
		string += byteString;
	}
	return string;
}

export type TopLevelDef = {
	// uuid: string,
	value: ASTnode,
};

export class DB {
	defs = new Map<string, TopLevelDef>();
	private evalQueue: string[] = [];
	// changeLog
	topLevelEvaluations: Indicator[] = [];
	errors: CompileError[] = [];
	currentDirectory: string = "";
	
	setDef(name: string, newDef: TopLevelDef) {
		this.defs.set(name, newDef);
		logger.log(LogType.DB, `set ${name}`);
	}
	
	getDef(name: string): TopLevelDef | null {
		if (name.startsWith("__builtin:")) {
			const builtin = builtins.get(name.slice("__builtin:".length));
			if (builtin != undefined) {
				return builtin;
			}
		}
		const def = this.defs.get(name);
		if (def != undefined) {
			return def;
		} else {
			return null;
		}
	}
	
	printDefs(): string {
		let text = "";
		this.defs.forEach((value, name) => {
			text += `\n${name} = ${value.value.print()}`;
		});
		return text;
	}
	
	addToEvalQueue(name: string) {
		if (!this.evalQueue.includes(name)) {
			this.evalQueue.push(name);
			logger.log(LogType.DB, `added to evalQueue: ${name}`);
		}
	}
	
	runEvalQueue() {
		logger.log(LogType.DB, `runEvalQueue ${JSON.stringify(this.evalQueue)}`);
		
		for (let i = 0; i < this.evalQueue.length; i++) {
			const name = this.evalQueue[i];
			const def = this.getDef(name);
			if (def == null) utilities.unreachable();
			
			const error = def.value.getType(new BuilderContext(this));
			if (error instanceof ASTnode_error && error.compileError) {
				this.errors.push(error.compileError);
				break;
			}
			const value = def.value.evaluate(new BuilderContext(this));
			
			// if (value instanceof ASTnodeType) {
			// 	value.id = name;
			// }
			def.value = value;
		}
		this.evalQueue = [];
	}
	
	addAST(AST: ASTnode[]) {
		for (let i = 0; i < AST.length; i++) {
			const ASTnode = AST[i];
			
			if (ASTnode instanceof ASTnode_command) {
				runCommand(this, ASTnode.text.split(" "));
				continue;
			} else if (ASTnode instanceof ASTnode_alias) {
				// const hash = hashString(JSON.stringify(ASTnode.value));
				// const uuid = getUUID();
				let name = ASTnode.left.print();
				if (this.currentDirectory != "") {
					name = this.currentDirectory + ":" + name;
				}
				// {
				// 	const error = ASTnode.getType(new BuilderContext(db));
				// 	if (error instanceof ASTnode_error && error.compileError) {
				// 		db.errors.push(error.compileError);
				// 		hasError = true;
				// 	}
				// }
				const newDef: TopLevelDef = {
					// uuid: uuid,
					value: ASTnode.value,
				};
				ASTnode.def = newDef;
				this.setDef(name, newDef);
				this.addToEvalQueue(name);
			} else {
				logger.log(LogType.DB, `found top level evaluation`);
				
				this.runEvalQueue();
				
				const location = ASTnode.location;
				{
					const error = ASTnode.getType(new BuilderContext(this));
					if (error instanceof ASTnode_error) {
						if (!error.compileError) {
							logger.log(LogType.DB, `!error.compileError`);
							break;
						}
						this.errors.push(error.compileError);
						continue;
					}
				}
				
				const result = ASTnode.evaluate(new BuilderContext(this));
				const codeGenContext = new CodeGenContext();
				codeGenContext.noPrintOrigin = true;
				const resultText = result.print(codeGenContext);
				
				this.topLevelEvaluations.push({ location: location, msg: `${resultText}` });
			}
		}
		
		this.runEvalQueue();
		
		// for (let i = 0; i < AST.length; i++) {
		// 	const ASTnode = AST[i];
			
		// 	if (ASTnode instanceof ASTnode_command || ASTnode instanceof ASTnode_alias) {
		// 		continue;
		// 	}
			
			
		// }
	}
}

export function getAlias(context: BuilderContext, name: string): ASTnode_alias | null {
	for (let i = context.aliases.length-1; i >= 0; i--) {
		const alias = context.aliases[i];
		if (alias.left instanceof ASTnode_identifier && alias.left.name == name) {
			return alias;
		}
	}
	
	return null;
}

export function unAlias(context: BuilderContext, name: string): ASTnode | null {
	{
		const alias = getAlias(context, name);
		if (alias) {
			return alias.value;
		}
	}
	
	{
		if (name.startsWith("~")) {
			const path = name.slice(1);
			const alias = context.db.getDef(path);
			if (alias != null) {
				return alias.value;
			}
		} else {
			const paths = context.db.currentDirectory.split(":");
			for (let i = 0; i < paths.length + 1; i++) {
				const pathList = paths.slice(0, i);
				pathList.push(name);
				const alias = context.db.getDef(pathList.join(":"));
				if (alias != null) {
					return alias.value;
				}
			}
		}
	}
	
	return null;
}

function runCommand(db: DB, args: string[]) {
	switch (args[0]) {
		case "cd": {
			const path = args[1];
			if (!path.startsWith("~")) utilities.TODO();
			db.currentDirectory = path.slice(1);
			return;
		}
	
		default: {
			utilities.TODO_addError();
		}
	}
}