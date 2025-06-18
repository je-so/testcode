// JavaScript Projects Organizer
// (c) 2025 Jörg Seebohn

// First version allows to create an ES module from a JavaScript file
// *.js -> *.mjs

import * as fs from "node:fs"

const jspo = (() => {

   /**
    * @typedef JSPO_Phase_Definition
    * @property {string} name
    * @property {string[]} [dependsOnPhases]
    * @property {string} [prePhase]
    * @property {string} [postPhase]
    *
    * @typedef JSPO_Phases_Definition
    * @property {string} defaultPhase
    * @property {JSPO_Phase_Definition[]} phases
    *
    * @typedef JSPO_BuildAction_Arg
    * @property {string} path
    * @property {string[]} dependsOnCommands
    * @property {string[]} dependsOnFiles
    *
    * @typedef {(arg:JSPO_BuildAction_Arg)=>Promise<void|string>} JSPO_BuildAction
    *
    * @typedef JSPO_CommandBuildRule_Definition
    * @property {string} command
    * @property {string[]} [dependsOnCommands]
    * @property {string[]} [dependsOnFiles]
    * @property {JSPO_BuildAction} action
    *
    * @typedef JSPO_FileBuildRule_Definition
    * @property {string} file
    * @property {string[]} [dependsOnCommands]
    * @property {string[]} [dependsOnFiles]
    * @property {JSPO_BuildAction} action
    *
    * @typedef {JSPO_CommandBuildRule_Definition|JSPO_FileBuildRule_Definition} JSPO_BuildRule_Definition
    */

   /**
    * @param {string} msg Message thrown as an Error.
    */
   const error = (msg) => { throw Error(msg) }

   class JSPO {
      #files = new Files()
      #predefinedPhases
      #buildPhases
      // TODO:
      // #projects

      constructor() {
         const predefinedPhases = new BuildPhases({defaultPhase:"",phases:[{name:""}]})
         this.#predefinedPhases = predefinedPhases
         this.#buildPhases = predefinedPhases
      }

      /** Allows to manipulate system files. */
      get files() { return this.#files }
      /** Get defined build phases in one object. The key reflects the name of the phase. */
      get phases() { return this.#buildPhases.phases }
      /** @return {BuildPhase} */
      get defaultPhase() { return this.#buildPhases.defaultPhase }
      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.#buildPhases.phase(name) }

      ///////////// TODO: start transformer-plugin ///////////////////
      /**
       * @param {string} content
       * @param {(string|[string,string])[]} exports
       * @return {string}
       */
      appendESModuleExport(content, exports) {
         const contentWithExports = content + "\n\nexport {\n"
                        + exports.filter(e => typeof e === "string").map(name => "   "+name).join(",\n")
                        + exports.filter(e => Array.isArray(e)).map(([name,as]) => `   ${name} as ${as}`).join(",\n")
                        + "\n}\n"
         return contentWithExports
      }
      ///////////// TODO: end transformer-plugin ///////////////////

      /** @param {JSPO_Phases_Definition} phasesDefinition */
      definePhases(phasesDefinition) {
         if (this.#buildPhases !== this.#predefinedPhases)
            error(`Call definePhases only once.`)
         this.#buildPhases = new BuildPhases(phasesDefinition)
      }

      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) { this.defaultPhase.addBuild(build) }

   }

   class Files {
      /**
       * @param {string} path
       * @return {Promise<string>}
       * */
      async loadString(path) {
         const file = Bun.file(path)
         if (await file.exists()) {
            return file.text()
         }
         return error(`File »${String(path)}« does not exist.`)
      }

      /**
       * @param {string} path
       * @param {string} content
       * @return {Promise<number>} The number of bytes written to disk.
       */
      async write(path, content) {
         const file = Bun.file(path)
         await file.exists() && error(`File »${String(path)}« exists use overwrite instead.`)
         return Bun.write(file, content)
      }

      /**
       * @param {string} path
       * @param {string} content
       * @return {Promise<number>} The number of bytes written to disk.
       */
      async overwrite(path, content) {
         const file = Bun.file(path)
         return Bun.write(file, content)
      }

      /**
       * Sets time of last modication of file to now or creates an empty file if it does not exist.
       * @param {string} path
       * @return {Promise<void>}
       */
      async touch(path) {
         return await this.exists(path)
               ? fs.promises.utimes(path, Date.now()/1000, Date.now()/1000)
               : Bun.write(path, "").then(() => {})
      }

      /** @return {Promise<number>} Time of last modification of file in milliseconds since Epoch. */
      async mtime(path) {
         if (await Bun.file(path).exists())
            return fs.statSync(path).mtimeMs
         return -1
      }

      async exists(path) { return Bun.file(path).exists() }

   }

   class BuildRule {
      /** @type {JSPO_BuildAction} */
      action
      /** @type {JSPO_BuildAction_Arg} */
      arg
      /**
       * @param {JSPO_BuildAction} action
       * @param {JSPO_BuildAction_Arg} arg
       */
      constructor(action, arg) {
         this.action = action
         this.arg = arg
      }
      async call() { return this.action(this.arg) }
   }

   class BuildNode {
      /** @type {string} */
      path
      /** @type {BuildNode[]} */
      dependsOn = []
      /** @type {undefined|BuildRule} */
      buildRule
      /** @param {string} path */
      constructor(path) {
         (typeof path === "string") || error(`Expect argument path of type string instead of »${typeof path}«.`)
         this.path = path
      }
      /** @return {boolean}  */
      get isVirtual() { return error("get isVirtual not implemented in subtype.") }
      /**
       * @param {number} mtime mtime of other node
       * @return {Promise<boolean>} True, if this node is modified and more recent than the given mtime.
       */
      async isNewer(mtime) { return error("isNewer not implemented in subtype.") }
      /** @return {Promise<boolean>}  */
      async existNode() { return error("existNode not implemented in subtype.") }
      /** @return {Promise<number>}  */
      async mtime() { return error("mtime not implemented in subtype.") }
      /** @param {BuildRule} buildRule */
      setBuildRule(buildRule) {
         this.buildRule && error(`Already defined build rule for build node with name »${this.path}«.`)
         this.buildRule = buildRule
      }
      /** @param {BuildStateNodes} state @return {Promise<boolean>}  */
      async needsRebuild(state) {
         const mtime = await this.mtime()
         const existNode = await this.existNode()
         if (!existNode) return true
         for (const source of this.dependsOn) {
            if (state.needsRebuild.has(source) || (!this.isVirtual && await source.isNewer(mtime)))
               return true
         }
         return false
      }

      /**
       * @typedef BuildStateNode
       * @property {BuildNode} node
       * @property {boolean} rebuild
       *
       * @typedef BuildStateNodes
       * @property {Set<BuildNode>} containedInSteps
       * @property {Set<BuildNode>} inValidation
       * @property {Set<BuildNode>} needsRebuild
       * @property {BuildStateNode[]} steps
       */

      static newBuildStateNodes() { return { containedInSteps:new Set(), inValidation:new Set(), needsRebuild:new Set(), steps:[] } }

      /** @param {BuildStateNodes} state @return {Promise<BuildStateNodes>} */
      async buildSteps(state=BuildNode.newBuildStateNodes()) {
         if (!state.containedInSteps.has(this)) {
            state.inValidation.has(this) && error(`${this.constructor.name} »${this.path}« is part of a cyclic dependency.`)
            state.inValidation.add(this)
            for (const source of this.dependsOn) {
               source.buildSteps(state)
            }
            const rebuild = await this.needsRebuild(state)
            state.steps.push( { node:this, rebuild } )
            if (rebuild) state.needsRebuild.add(this)
            state.inValidation.delete(this)
            state.containedInSteps.add(this)
         }
         return state
      }
   }

   class FileNode extends BuildNode {
      get isVirtual() { return false }
      /** @param {number} mtime @return {Promise<boolean>} */
      async isNewer(mtime) { return mtime < await this.mtime() }
      async existNode() { return jspo.files.exists(this.path) }
      async mtime() { return jspo.files.mtime(this.path) }
   }

   class CommandNode extends BuildNode {
      get isVirtual() { return true }
      /** @param {number} mtime @return {Promise<boolean>} */
      async isNewer(mtime) { return false }
      /** Returns true (=> prevents rebuild due to non existance) if there are dependencies. */
      async existNode() { return this.dependsOn.length != 0 }
      async mtime() { return -1 }
   }

   class BuildNodes {
      #validated = false
      /** @type {Map<string,BuildNode>} Stores all CommandNodes/FileNodes indexed by name/path which must be build. */
      buildNodes = new Map()
      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) {
         /** @param {string} path @param {boolean} isCommand */
         const getNode = (path, isCommand) => {
            const node = this.buildNodes.get(path) ?? (isCommand ? new CommandNode(path) : new FileNode(path))
            this.buildNodes.has(path) || this.buildNodes.set(path, node)
            return node
         }
         /** @param {string[]} dependsOn @param {string} argName */
         const checkType = (dependsOn, argName) => {
            Array.isArray(dependsOn) || error(`Expect argument build.${argName} of type Array instead of »${typeof dependsOn}«.`)
            dependsOn.forEach( (path,i) => (typeof path === "string") || error(`Expect argument build.${argName}[${i}] of type string instead of »${typeof path}«.`))
         }
         const isCommand = "command" in build
         const path = isCommand ? build.command : build.file
         typeof path === "string" || error(`Expect argument build.${isCommand?"command":"file"} of type string instead of »${typeof path}«.`)
         const dependsOnCommands = build.dependsOnCommands ?? []
         const dependsOnFiles = build.dependsOnFiles ?? []
         const action = build.action
         typeof action === "function" || error(`Expect argument build.action of type function instead of »${typeof action}«.`)
         checkType(dependsOnCommands,"dependsOnCommands")
         checkType(dependsOnFiles,"dependsOnFiles")
         const buildNode = getNode(path, isCommand)
         !buildNode.buildRule || error(`Expect argument build.${isCommand?"command":"file"} = »${path}« to be unique.`)
         buildNode.setBuildRule(new BuildRule(action, { path, dependsOnCommands, dependsOnFiles }))
         for (const isDependCommand of [true, false]) {
            for (const path of (isDependCommand ? dependsOnCommands : dependsOnFiles)) {
               const source = getNode(path, isDependCommand)
               if (!buildNode.dependsOn.includes(source))
                  buildNode.dependsOn.push(source)
            }
         }
         this.#validated = false
      }
      /** @return {Promise<void>} */
      async build() {
         const steps = await this.buildSteps()
         for (const step of steps) {
            const node = step.node
            if (step.rebuild && node.buildRule) {
               await node.buildRule.call()
               if (!node.isVirtual)
                  await jspo.files.touch(node.path)
            }
         }
      }
      /** @return {Promise<BuildStateNode[]>} */
      async buildSteps(state=BuildNode.newBuildStateNodes()) {
         this.validate()
         for (const node of this.buildNodes.values())
            await node.buildSteps(state)
         return state.steps
      }
      async validate() {
         if (this.#validated) return
         this.#validated = true
         for (const node of this.buildNodes.values())
            if (!node.buildRule && (node.isVirtual || !(await Bun.file(node.path).exists())))
               error(`Undefined build rule for ${node.isVirtual?"command":"file"} »${node.path}«.`)
      }
   }

   class BuildPhase {
      /** @type {string} */
      name
      /** @type {BuildNodes} */
      buildNodes
      /** @type {BuildPhase[]} */
      dependsOnPhases
      /** @type {undefined|BuildPhase} */
      prePhase
      /** @type {undefined|BuildPhase} */
      postPhase

      /**
       * @param {string} name
       */
      constructor(name) {
         this.name = name
         this.buildNodes = new BuildNodes()
         this.dependsOnPhases = []
      }

      /**
       * @param {BuildPhase[]} dependsOnPhases
       * @param {undefined|BuildPhase} prePhase
       * @param {undefined|BuildPhase} postPhase
       */
      setDependencies(dependsOnPhases, prePhase, postPhase) {
         this.dependsOnPhases = [...dependsOnPhases]
         this.prePhase = prePhase
         this.postPhase = postPhase
      }

      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) { this.buildNodes.addBuild(build) }

      /**
       * @typedef BuildStatePhase
       * @property {string} calledFrom
       * @property {BuildPhase} phase
       * @property {boolean} skip
       *
       * @typedef BuildStatePhases
       * @property {string} calledFrom
       * @property {Set<BuildPhase>} containedInSteps
       * @property {Set<BuildPhase>} inValidation
       * @property {BuildStatePhase[]} phases
       */

      /** @return {BuildStatePhases} */
      static newBuildStatePhases() { return { calledFrom:"", containedInSteps:new Set(), inValidation:new Set(), phases:[] } }

      /** @return {Promise<BuildStateNode[]>} */
      async buildSteps(state=BuildPhase.newBuildStatePhases()) {
         const buildSteps = []
         this.buildPhases(state)
         for (const step of state.phases) {
            if (step.skip) continue
            buildSteps.push(...(await step.phase.buildStepsOnlyThis()))
         }
         return buildSteps
      }

      /** @return {Promise<BuildStateNode[]>} */
      async buildStepsOnlyThis() { return this.buildNodes.buildSteps() }

      /** @return {Promise<void>} */
      async build(state=BuildPhase.newBuildStatePhases()) {
         this.buildPhases(state)
         for (const step of state.phases) {
            this.logStep(step)
            if (step.skip) continue
            await step.phase.buildOnlyThis()
         }
      }

      /** @return {Promise<void>} */
      async buildOnlyThis() { return this.buildNodes.build() }

      /** @param {BuildStatePhases} state @return {BuildStatePhases} */
      buildPhases(state=BuildPhase.newBuildStatePhases()) {
         if (!state.containedInSteps.has(this)) {
            state.inValidation.has(this) && error(`Phase »${this.name}« is part of a cyclic dependency.`)
            state.inValidation.add(this)
            const subState = { ...state, calledFrom:state.calledFrom + "/" + this.name }
            for (const phase of this.dependsOnPhases)
               phase.buildPhases(subState)
            state.inValidation.delete(this)
            this.buildPhasesPreSelfPost(state)
            state.containedInSteps.add(this)
         }
         else
            state.phases.push({ calledFrom:state.calledFrom, phase:this, skip:true })
         return state
      }

      /** @param {BuildStatePhases} state @return {BuildStatePhases} */
      buildPhasesPreSelfPost(state=BuildPhase.newBuildStatePhases()) {
         state.inValidation.has(this) && error(`Phase »${this.name}« is part of a cyclic dependency.`)
         state.inValidation.add(this)
         const subState = { ...state, calledFrom:state.calledFrom + "/" + this.name }
         this.prePhase && this.prePhase.buildPhasesPreSelfPost(subState)
         state.phases.push({ calledFrom:state.calledFrom, phase:this, skip:false })
         this.postPhase && this.postPhase.buildPhasesPreSelfPost(subState)
         state.inValidation.delete(this)
         return state
      }

      /** @param {BuildStatePhase} step */
      logStep(step) { console.log(`\nPhase ::${step.phase.name}:: ${step.skip?"skip":"build"} (${step.calledFrom?"parent ":"root"}${step.calledFrom})`) }
   }

   class BuildPhases {
      /** @type {BuildPhase} */
      defaultPhase
      /** @type {{}|{[name:string]:BuildPhase}} */
      phases

      /** @param {JSPO_Phases_Definition} phasesDefinition */
      constructor(phasesDefinition) {
         const phases = {}
         /** @param {string} name @param {number} i */
         const getPhase = (name, i, attrName) => {
            const phase = phases[String(name)]
            return phase ?? error(`Expect argument phasesDefinition.phases[${i}].${attrName} = »${String(name)}« to reference an existing phase.`)
         }
         if (phasesDefinition === null || typeof phasesDefinition !== "object")
            error(`Expect argument phasesDefinition of type object instead of »${phasesDefinition === null ? "null" : typeof phasesDefinition}«.`)
         if (typeof phasesDefinition.defaultPhase !== "string")
            error(`Expect argument phasesDefinition.defaultPhase of type string instead of »${typeof phasesDefinition.defaultPhase}«.`)
         if (!Array.isArray(phasesDefinition.phases))
            error(`Expect argument phasesDefinition.phases of type Array instead of »${typeof phasesDefinition.phases}«.`)
         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            const name = phaseDefinition.name
            if (typeof name !== "string")
               error(`Expect argument phasesDefinition.phases[${i}].name of type string instead of »${typeof name}«.`)
            if (phases[name])
               error(`Expect argument phasesDefinition.phases[${i}].name = »${name}« to be unique.`)
            const phase = new BuildPhase(name)
            phases[phase.name] = phase
         })
         if (phases[phasesDefinition.defaultPhase] === undefined)
            error(`Expect argument phasesDefinition.defaultPhase = »${phasesDefinition.defaultPhase}« to reference an existing phase with same name.`)
         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            if (phaseDefinition.dependsOnPhases && !Array.isArray(phaseDefinition.dependsOnPhases))
               error(`Expect argument phasesDefinition.phases[${i}].dependsOnPhases of type Array instead of »${typeof phaseDefinition.dependsOnPhases}«.`)
            const dependsOnPhases = (phaseDefinition.dependsOnPhases ?? []).map( (name, i2) =>
               getPhase(name,i,`dependsOnPhases[${i2}]`)
            )
            const prePhase = phaseDefinition.prePhase === undefined ? undefined : getPhase(phaseDefinition.prePhase,i,"prePhase")
            const postPhase = phaseDefinition.postPhase === undefined ? undefined : getPhase(phaseDefinition.postPhase,i,"postPhase")
            phases[phaseDefinition.name].setDependencies(dependsOnPhases,prePhase,postPhase)
         })
         const buildState = BuildPhase.newBuildStatePhases()
         Object.values(phases).forEach( phase => phase.buildPhases(buildState))
         this.defaultPhase = phases[phasesDefinition.defaultPhase]
         this.phases = phases
      }

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.phases[name] ?? error(`Phase »${String(name)}« does no exist.`) }
   }

   return new JSPO()
})()

jspo.definePhases({
   defaultPhase: "build-javascript-modules",
   phases: [
      { name: "pre-clean" },
      { name: "post-clean", },
      { name: "xxx", prePhase: "pre-clean", postPhase: "post-clean" },
      { name: "clean", dependsOnPhases: ["xxx"], prePhase: "pre-clean", postPhase: "post-clean" },
      { name: "build-javascript-modules", dependsOnPhases: ["clean","xxx"] },
   ]
})

jspo.addBuild({ file:"test.mjs", dependsOnFiles:["html-snippets/test.js"], action: async () => {
   console.log("Build test.mjs")
   await jspo.files.overwrite("test.mjs",
      jspo.appendESModuleExport(
            await jspo.files.loadString("html-snippets/test.js"),
            [ "TEST", "RUN_TEST", "SUB_TEST", "END_TEST", "RESET_TEST", "NEW_TEST" ]
      ),
   )
}})

// default-phase
jspo.addBuild({ file:"xxx1", dependsOnFiles:["xxx2","xxx3"], action: async ()=>console.log("build xxx1") })
jspo.addBuild({ file:"xxx10", dependsOnFiles:["xxx1","xxx3"], action: async ()=>console.log("build xxx10") })
jspo.addBuild({ command:"test.mjs postprocessor", dependsOnFiles:["test.mjs"], action:async ()=>console.log("test.mjs postprocessor") })

const cleanPhase = jspo.phase("clean")
cleanPhase.addBuild({ command:"remove-files", dependsOnCommands:[], action:async ()=>console.log("remove files") })
cleanPhase.addBuild({ command:"remove-dirs", dependsOnCommands:["remove-files"], action:async ()=>console.log("remove directories") })
jspo.phase("pre-clean").addBuild({ command:"pre-clean", action:async ()=>console.log("pre-clean commands") })
jspo.phase("post-clean").addBuild({ command:"post-clean", action:async ()=>console.log("post-clean commands") })
jspo.phase("xxx").addBuild({ command:"xxx", action:async ()=>console.log("xxx commands") })

await cleanPhase.build()

await jspo.defaultPhase.buildSteps()
   .then( steps => console.log(steps))
   .then( () => jspo.defaultPhase.build())
   .catch( e => console.log("error",e))


