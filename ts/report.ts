import * as utilities from "./utilities";
import { SourceLocation } from "./types";

const lineNumberPadding = 4;
const indicatorTextWindowSize = 1;

export type Indicator = {
	location: SourceLocation,
	msg: string,
}

// TODO: This can read the same file twice, if there are two indicators in a file.
export function getIndicatorText(indicator: Indicator, printPath: boolean, fancyIndicators: boolean, startAtNextLine: boolean): string {
	let errorText = "";
	
	if (!fancyIndicators) {
		if (indicator.location != "builtin") {
			if (printPath) {
				errorText += `${indicator.location.path}:${indicator.location.line} -> ${indicator.msg}`;
			} else {
				errorText += `line:${indicator.location.line} -> ${indicator.msg}`;
			}
			return errorText;
		} else {
			errorText += `builtin -> ${indicator.msg}`;
			return errorText;
		}
	}
	
	if (indicator.location != "builtin") {
		const text = utilities.readFile(indicator.location.path);
		// console.log("compileError location:", location);
		
		if (printPath) {
			errorText += `at ${indicator.location.path}:${indicator.location.line}\n`;
		} else {
			errorText += `${indicator.location.line}\n`;
		}
		
		let i = 0;
		let line = 1;
		
		function next(): string {
			if (text[i] == "\n") {
				line++;
			}
			return text[i++];
		}
		
		function writeLine(): number {
			let size = 0
			let lineText = "";
			if (indicator.location != "builtin") {
				lineText += line.toString().padStart(lineNumberPadding, "0");
				lineText += " |";
				size = lineText.length;
				
				let lineI = 0;
				while (i < text.length) {
					const char = next();
					
					lineI++;
					
					if (char == "\n") {
						break;
					}
					
					if (char == "\t") {
						lineText += "    ";
					} else {
						lineText += char;
					}
					
					if (lineI == indicator.location.startColumn) {
						size = lineText.length - 1;
					}
				}
				lineText += "\n";
			} else {
				utilities.unreachable();
			}
			errorText += lineText;
			
			return size;
		}
		
		while (i < text.length) {
			if (line == indicator.location.line) {
				const size = writeLine();
				for (let index = 0; index < size; index++) {
					errorText += " ";
				}
				for (let index = 0; index < indicator.location.endColumn - indicator.location.startColumn + 1; index++) {
					errorText += "^";
				}
				let endText = "";
				if (startAtNextLine) {
					endText += "\n";
				} else {
					endText += " ";
				}
				endText += `${indicator.msg}`;
				if (startAtNextLine) {
					endText = endText.replaceAll("\n", "\n ");
				}
				errorText += endText;
				break;
			} else if (
				line > indicator.location.line - indicatorTextWindowSize - 1 &&
				line < indicator.location.line + indicatorTextWindowSize + 1
			) {
				writeLine();
			} else {
				next();
			}
		}
		
		errorText += `\n`;	
	} else {
		errorText += `at builtin\n\n`;
	}
	
	return errorText;
}

export class CompileError {
	msg: string
	private indicators: Indicator[]
	
	constructor(msg: string) {
		this.msg = msg;
		this.indicators = [];
	}
	
	public indicator(location: SourceLocation, msg: string): CompileError {
		this.indicators.push({
			location: location,
			msg: msg,
		});
		return this;
	}
	
	public getText(printPath: boolean, fancyIndicators: boolean): string {
		let text = `error: ${this.msg}\n`;
		for (let i = 0; i < this.indicators.length; i++) {
			const indicator = this.indicators[i];
			
			if (indicator.location == "builtin") continue;
			
			text += getIndicatorText(indicator, printPath, fancyIndicators, false);
			if (!fancyIndicators && this.indicators[i + 1]) {
				text += "\n";
			}
		}
		return text;
	}
}