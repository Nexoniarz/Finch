// The Finch extension: starts `finch lsp` for editor features and adds Run / Build.
const vscode = require('vscode');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

let client = null;

function finchPath() {
    return vscode.workspace.getConfiguration('finch').get('path') || 'finch';
}

async function startServer(context) {
    if (!vscode.workspace.getConfiguration('finch').get('languageServer')) return;
    const command = finchPath();
    client = new LanguageClient(
        'finch',
        'Finch',
        { command, args: ['lsp'], transport: TransportKind.stdio },
        { documentSelector: [{ scheme: 'file', language: 'finch' }] }
    );
    try {
        await client.start();
    } catch (err) {
        client = null;
        const pick = await vscode.window.showErrorMessage(
            `Finch: couldn't start "${command} lsp". Is Finch installed? You can set its path in the settings.`,
            'Open Settings'
        );
        if (pick) vscode.commands.executeCommand('workbench.action.openSettings', 'finch.path');
    }
}

// Run or build the current file as a task: no shell quoting, and the problem matcher
// turns compile errors into entries in the Problems panel.
async function finchTask(verb) {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== 'finch') {
        vscode.window.showWarningMessage('Finch: open a .fch file first.');
        return;
    }
    if (editor.document.isDirty) await editor.document.save();
    const file = editor.document.uri.fsPath;
    const folder = vscode.workspace.getWorkspaceFolder(editor.document.uri);
    const task = new vscode.Task(
        { type: 'finch', verb, file },
        folder || vscode.TaskScope.Workspace,
        `${verb} ${require('path').basename(file)}`,
        'finch',
        new vscode.ProcessExecution(finchPath(), [verb, file], { cwd: require('path').dirname(file) }),
        '$finch'
    );
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, focus: verb === 'run', clear: true };
    vscode.tasks.executeTask(task);
}

async function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand('finch.run', () => finchTask('run')),
        vscode.commands.registerCommand('finch.build', () => finchTask('build')),
        vscode.commands.registerCommand('finch.restartServer', async () => {
            if (client) await client.stop();
            await startServer(context);
        }),
        vscode.workspace.onDidChangeConfiguration(async (e) => {
            if (e.affectsConfiguration('finch')) {
                if (client) await client.stop();
                await startServer(context);
            }
        })
    );
    await startServer(context);
}

async function deactivate() {
    if (client) await client.stop();
}

module.exports = { activate, deactivate };
