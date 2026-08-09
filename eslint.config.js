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
      "runtime/HTML/**/jszip*.js"
    ]
  },
  {
    files: ["runtime/**/*.js"],
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
        confirm: "readonly",
        console: "readonly",
        document: "readonly",
        event: "readonly",
        external: "readonly",
        location: "readonly",
        navigator: "readonly",
        prompt: "readonly",
        showModalDialog: "readonly",
        window: "readonly"
      }
    },
    rules: {
      "no-dupe-args": "error",
      "no-dupe-keys": "error",
      "no-self-assign": "error",
      "no-unreachable": "error",
      "no-unexpected-multiline": "error",
      "use-isnan": "error",
      "valid-typeof": "error"
    }
  }
];
