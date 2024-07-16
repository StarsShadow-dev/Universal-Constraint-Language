import crypto from "crypto";

import { ASTnode } from "./parser";
import logger, { LogType } from "./logger";
import utilities from "./utilities";

export type topLevelDef = {
	// uuid: string,
	name: string,
	AST: ASTnode[],
};

export type DB = {
	defs: topLevelDef[],
	// changeLog
};

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

export function getDefFromList(list: topLevelDef[], name: string): topLevelDef | null {
	for (let i = 0; i < list.length; i++) {
		const def = list[i];
		if (def.name == name) {
			return def;
		}
	}

	return null;
}

export type ResolveAliasesContext = {
	defList: topLevelDef[]
};

export function resolveAliases(context: ResolveAliasesContext, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		switch (ASTnode.kind) {
			case "identifier": {
				ASTnode.name;
				const def = getDefFromList(context.defList, ASTnode.name);
				if (!def) {
					throw utilities.TODO();
				}
				ASTnode.name = def.name;
				
				break;
			}
			
			case "call": {
				resolveAliases(context, [ASTnode.left]);
				
				break;
			}
		}
	}
}

export function addToDB(db: DB, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			// const hash = hashString(JSON.stringify(ASTnode.value)); // TODO: locations...
			const uuid = getUUID();
			db.defs.push({
				// uuid: uuid,
				name: ASTnode.name,
				AST: [],
			});
			logger.log(LogType.addToDB, `added ${ASTnode.name} as ${uuid}`);
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			const def = getDefFromList(db.defs, ASTnode.name);
			if (!def) throw utilities.unreachable();
			const value = ASTnode.value;
			
			switch (value.kind) {
				case "function": {
					const codeBlock = value.codeBlock;
					// TODO: resolveAliases()
					def.AST = codeBlock;
					logger.log(LogType.addToDB, `added AST to ${def.name}`);
					
					break;
				}
			}
		} else {
			logger.log(LogType.addToDB, `got not alias`);
			
			const codeBlock = [ASTnode];
			
			resolveAliases({
				defList: db.defs,
			}, codeBlock);
			
			debugger;
		}
	}
}