// FBE user scripts execute in the legacy Microsoft JScript host.  Keep this
// profile separate from any future Node.js tooling that may be added under tools/.
export default [
  {
    ignores: [
      "out/**",
      "tmp/**",
      "build/**",
      "third_party/**",
      "runtime/HTML/**/jquery*.js",
      "runtime/HTML/**/jquery-*.js",
      "runtime/HTML/**/jszip*.js",
      "runtime/**/generated/**",
      "runtime/**/vendor/**"
    ]
  },
  {
    files: ["runtime/**/*.js", "runtime/**/*.html", "runtime/**/*.htm"],
    languageOptions: {
      ecmaVersion: 3,
      sourceType: "script",
      globals: {
        ActiveXObject: "readonly",
        CollectGarbage: "readonly",
        Enumerator: "readonly",
        GetObject: "readonly",
        VBArray: "readonly",
        alert: "readonly",
        AskYesNo: "readonly",
        clearInterval: "readonly",
        clearTimeout: "readonly",
        confirm: "readonly",
        console: "readonly",
        document: "readonly",
        event: "readonly",
        external: "readonly",
        frames: "readonly",
        GoTo: "readonly",
        history: "readonly",
        IDABORT: "readonly",
        IDCANCEL: "readonly",
        IDIGNORE: "readonly",
        IDNO: "readonly",
        IDOK: "readonly",
        IDRETRY: "readonly",
        IDYES: "readonly",
        Image: "readonly",
        InflateIt: "readonly",
        InputBox: "readonly",
        location: "readonly",
        MsgBox: "readonly",
        navigator: "readonly",
        opener: "readonly",
        parent: "readonly",
        prompt: "readonly",
        screen: "readonly",
        setInterval: "readonly",
        setTimeout: "readonly",
        showModalDialog: "readonly",
        showModelessDialog: "readonly",
        self: "readonly",
        top: "readonly",
        WScript: "readonly",
        window: "readonly"
      }
    },
    rules: {
      "no-dupe-args": "error",
      "no-dupe-keys": "error",
      "no-self-assign": "error",
      "no-unreachable": "error",
      "no-unexpected-multiline": "error",
      "no-redeclare": ["error", { builtinGlobals: false }],
      "no-undef": "error",
      "use-isnan": "error",
      "valid-typeof": "error"
    }
  }
];
