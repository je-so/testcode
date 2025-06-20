// JavaScript Projects Organizer
// (c) 2025 Jörg Seebohn

// First version allows to create an ES module from a JavaScript file
// *.js -> *.mjs

// !! execute as unprivileged user to prevent any accidental damage to the file system !!
// run with
// > bun run jspo.js

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

   /** @param {string} msg Message thrown as an Error. @param {Error} [cause] */
   const error = (msg,cause) => { throw (cause ? Error(msg,{cause}) : Error(msg) ) }

   class JSPO {
      /** @type {Files} */
      #files
      /** @type {BuildPhases} */
      #predefinedPhases
      /** @type {BuildPhases} */
      #buildPhases
      // TODO:
      // #projects

      constructor() {
         const predefined = new BuildPhases({defaultPhase:"",phases:[{name:""}]})
         this.#files = new Files()
         this.#predefinedPhases = predefined
         this.#buildPhases = predefined
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

   /**
    * Base class of every build node which depends on other nodes of same (sub-)type.
    */
   class DependentNode {
      /** @type {string} */
      path
      /** @type {DependentNode[]} */
      dependentOn

      /** @param {string} path */
      constructor(path) {
         typeof path === "string" || error(`Expect argument path of type string instead of ${typeof path}.`)
         this.path = path
         this.dependentOn = []
      }

      /** @return {DependentNode[]} */
      get typedDependentOn() { return this.dependentOn }

      /** @param {DependentNode[]} dependentOn @param {typeof DependentNode} nodeType */
      setDependency(dependentOn, nodeType) {
         /** @param {any} node @param {number} i */
         const checkType = (node,i) => { if (!(node instanceof nodeType)) error(`Expect argument dependentOn[${i}] of type ${nodeType.name} instead of ${node?.constructor?.name ?? typeof node}.`) }
         Array.isArray(dependentOn) || error(`Expect argument dependentOn of type Array instead of ${typeof dependentOn}.`)
         dependentOn.forEach(checkType)
         this.dependentOn = [...new Set(dependentOn)]
      }
   }

   /**
    * Build step which contains a reference to a node and a flag (needUpdate) if node has to be processed.
    * Steps which could be executed in parallel has the same level number (starting from 0 counting upwards).
    */
   class ProcessingStep {
      /** @type {DependentNode} */
      node
      /** @type {ProcessingStep[]} */
      reverseDependentOn
      /** @type {number} */
      unprocessedDependentOn
      /** @type {number} */
      level
      /** @type {boolean} true, if node has to be processed cause of not beeing up-to-date. */
      needUpdate

      /** @param {DependentNode} node */
      constructor(node) {
         this.node = node
         this.reverseDependentOn = []
         this.unprocessedDependentOn = node.dependentOn.length
         this.level = 0
         this.needUpdate = true
      }
   }

   /**
    * Allows to describe a ProcessingStep with correct subtype of DependentNode.
    * @template {DependentNode} T
    * @typedef TProcessingStep
    * @property {T} node
    * @property {TProcessingStep<T>[]} reverseDependentOn
    * @property {number} unprocessedDependentOn
    * @property {number} level
    * @property {boolean} needUpdate
    */

   /**
    * Base class of all set of build nodes which depend on each other (dependency graph).
    */
   class DependencyGraph {

      ////////////////////////////
      // Overwritten in Subtype //
      ////////////////////////////

      /** @return {Promise<void>} */
      async validate() { error(`validate not overwritten in subtype.`) }

      /** @param {number} level @return {Promise<ProcessingStep[]>} */
      async getProcessingSteps(level=0) { return error(`getProcessSteps not overwritten in subtype.`) }

      /////////////////////////////////////////////////
      // Default Implementations (use by delegation) //
      /////////////////////////////////////////////////

      /**
       * @template T
       * @param {string} path
       * @param {Map<string,T>} nodes
       * @param {(path:string)=>T} newNode
       * @return {T}
       */
      static ensureNode(path, nodes, newNode) {
         const node = nodes.get(path)
         if (node) return node
         this.validated = false
         const node2 = newNode(path)
         return nodes.set(path, node2), node2
      }

      /**
       * @template {DependentNode} T
       * @param {T} node
       * @return {Map<string,T>}
       */
      static getDependentNodes(node) {
         /** @type {Map<string,DependentNode>} */
         const nodes = new Map([[node.path,node]])
         /** @type {DependentNode[]} */
         const unprocessed = [node]
         for (;;) {
            const nextNode = unprocessed.shift()
            if (nextNode === undefined) break
            const deps = nextNode.dependentOn
            unprocessed.push(...deps)
            deps.forEach(node => nodes.set(node.path,node))
         }
         /** @type {Map<string,any>} */
         const anyNodes = nodes
         return anyNodes
      }


      ////////////////
      // Validation //
      ////////////////

      /** @param {Map<string,DependentNode>} nodes */
      static validate(nodes) {
         this.validateAllNodesContained(nodes)
         this.validateNoCycles(nodes)
      }

      /** @param {Map<string,DependentNode>} nodes */
      static validateAllNodesContained(nodes) {
         for (const node of nodes.values()) {
            node.dependentOn.forEach( (node, i) => {
               if (nodes.get(node.path) !== node) {
                  error(`Node »node.path«.dependentOn[${i}] references uncontained node ${node.path}.`)
               }
            })
         }
      }

      /** @param {Map<string,DependentNode>} nodes */
      static validateNoCycles(nodes) { this.getProcessingSteps(nodes, 0, (node)=>new ProcessingStep(node)) }

      ///////////////////////////////////
      // Layout Nodes in Process Order //
      ///////////////////////////////////

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes
       * @param {number} level
       * @param {(node:DependentNode)=>TProcessingStep<T>} newProcessingStep
       * @return {TProcessingStep<T>[]}
       */
      static getProcessingSteps(nodes, level, newProcessingStep) {
         return this.orderSteps(this.wrapNodes(nodes, newProcessingStep),level)
      }

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes
       * @param {(node:DependentNode)=>TProcessingStep<T>} newProcessingStep
       * @return {TProcessingStep<T>[]}
       */
      static wrapNodes(nodes, newProcessingStep) {
         const steps = new Map()
         /** @param {DependentNode} node */
         const getStep = (node) => {
            const step = steps.get(node)
            if (step) return step
            const newStep = newProcessingStep(node)
            return steps.set(node, newStep), newStep
         }
         for (const node of nodes.values()) {
            const step = getStep(node)
            for (const source of node.dependentOn) {
               const srcStep = getStep(source)
               srcStep.reverseDependentOn.push(step)
            }
         }
         return [...steps.values()]
      }

      /**
       * @template {DependentNode} T
       * @param {TProcessingStep<T>[]} steps
       * @param {number} level
       * @return {TProcessingStep<T>[]}
       */
      static orderSteps(steps, level) {
         const orderedSteps = []
         let nextSteps = steps.filter( step => step.unprocessedDependentOn === 0)
         while (orderedSteps.length < steps.length) {
            if (nextSteps.length === 0) {
               const step = steps.find(step => step.unprocessedDependentOn>0)
               error(`${step?.node.constructor?.name ?? "Node"} »${step?.node.path ?? "???"}« is part of a dependency cycle.`)
            }
            nextSteps.forEach( step => {
               step.level = level
               orderedSteps.push(step)
            })
            const nextNextSteps = []
            for (const step of nextSteps)
               for (const follow of step.reverseDependentOn)
                  if (-- follow.unprocessedDependentOn <= 0)
                     nextNextSteps.push(follow)
            level ++, nextSteps = nextNextSteps
         }
         return orderedSteps
      }
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

   class BuildNode extends DependentNode {
      /** @type {undefined|BuildRule} */
      buildRule
      /** @param {string} path */
      constructor(path) {
         super(path)
      }
      /** @return {BuildNode[]} */
      get typedDependentOn() {
         /** @type {any} */
         const typed = this.dependentOn
         return typed
      }
      /** @return {boolean}  */
      get isVirtual() { return error("get isVirtual not implemented in subtype.") }
      /** @return {string}  */
      get type() { return error("type not implemented in subtype.") }
      /** @return {Promise<undefined|number>}  */
      async mtime() { return error("mtime not implemented in subtype.") }
      /** @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(updateNodes) { return error("needsUpdate not implemented in subtype.") }
      /** @param {BuildRule} buildRule */
      setBuildRule(buildRule) { this.buildRule = buildRule }
      /** @param {BuildNode[]} dependentOn  */
      setDependency(dependentOn) { super.setDependency(dependentOn, BuildNode) }
   }

   class FileNode extends BuildNode {
      get isVirtual() { return false }
      get type() { return "file" }
      async mtime() { return jspo.files.mtime(this.path) }
      /** @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(updateNodes) {
         const existNode = await jspo.files.exists(this.path)
         if (!existNode) return true
         const mtime = await this.mtime()
         for (const source of this.typedDependentOn)
            if (updateNodes.has(source) || mtime < Number(await source.mtime()))
               return true
         return false
      }
   }

   class CommandNode extends BuildNode {
      get isVirtual() { return true }
      get type() { return "command" }
      async mtime() { return undefined }
      /** @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(updateNodes) {
         const dependentOn = this.typedDependentOn
         if (dependentOn.length === 0) return true
         for (const source of dependentOn)
            if (updateNodes.has(source))
               return true
         return false
      }
   }

   class BuildNodes extends DependencyGraph {
      /** @type {Map<string,BuildNode>} Stores all CommandNodes/FileNodes indexed by name/path which must be build. */
      buildNodes = new Map()
      /** @type {boolean} */
      validated

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      constructor() {
         super()
         this.validated = false
      }

      async validate() {
         if (this.validated) return
         DependencyGraph.validate(this.buildNodes)
         for (const node of this.buildNodes.values())
            if (!node.buildRule && (node.isVirtual || !(await Bun.file(node.path).exists())))
               error(`Undefined build rule for ${node.type} »${node.path}«.`)
         this.validated = true
      }

      /** @param {BuildNode} node @return {TProcessingStep<BuildNode>} */
      static newProcessingStep(node) {
         /** @type {any} */
         const step = new ProcessingStep(node)
         return step
      }

      /** @param {number} level @return {Promise<TProcessingStep<BuildNode>[]>} */
      async getProcessingSteps(level=0) {
         this.validate()
         const updateNodes = new Set()
         const steps = DependencyGraph.getProcessingSteps(this.buildNodes,level,BuildNodes.newProcessingStep)
         for (const step of steps) {
            if (step.needUpdate) {
               step.needUpdate = await step.node.needsUpdate(updateNodes)
               if (step.needUpdate) updateNodes.add(step.node)
            }
         }
         return steps
      }

      ///////////

      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) {
         this.validated = false
         /** @param {string} path @param {boolean} isCommand */
         const getNode = (path, isCommand) => {
            return DependencyGraph.ensureNode(path, this.buildNodes, () => isCommand ? new CommandNode(path) : new FileNode(path))
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
         buildNode.buildRule && error(`Expect argument build.${isCommand?"command":"file"} = »${path}« to be unique.`)
         buildNode.setBuildRule(new BuildRule(action, { path, dependsOnCommands, dependsOnFiles }))
         const commandNodes = dependsOnCommands.map(path => getNode(path, true))
         const fileNodes = dependsOnFiles.map(path => getNode(path, false))
         buildNode.setDependency(commandNodes.concat(fileNodes))
      }

      ///////////

      /** @return {Promise<void>} */
      async build() {
         const steps = await this.getProcessingSteps()
         for (const step of steps) {
            const node = step.node
            if (step.needUpdate && node.buildRule) {
               await node.buildRule.call()
               if (!node.isVirtual)
                  await jspo.files.touch(node.path)
            }
         }
      }

      /** @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps() { return this.getProcessingSteps() }
   }

   class BuildPhase extends DependentNode {
      /** @type {BuildNodes} */
      buildNodes
      /** @type {undefined|BuildPhase} */
      prePhase
      /** @type {undefined|BuildPhase} */
      postPhase

      /**
       * @param {string} name
       */
      constructor(name) {
         super(name)
         this.buildNodes = new BuildNodes()
         this.dependsOnPhases = []
      }

      /** @type {string} */
      get name() { return this.path }

      /** @return {BuildPhase[]} */
      get typedDependentOn() {
         /** @type {any} */
         const typed = this.dependentOn
         return typed
      }

      /**
       * @param {BuildPhase[]} dependentOn
       */
      setDependency(dependentOn) { super.setDependency(dependentOn, BuildPhase) }

      /**
       * @param {undefined|BuildPhase} prePhase
       * @param {undefined|BuildPhase} postPhase
       * @return {BuildPhase}
       */
      setPrePost(prePhase, postPhase) {
         this.prePhase = prePhase
         this.postPhase = postPhase
         return this
      }

      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) { this.buildNodes.addBuild(build) }

      /** @param {number} level @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level=0) {
         /** @type {Map<string,BuildPhase>} */
         const phases = DependencyGraph.getDependentNodes(this)
         /** @type {TProcessingStep<BuildPhase>[]} */
         const steps = DependencyGraph.getProcessingSteps(phases, level, BuildPhases.newProcessingStep)
         return steps
      }

      /** @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps() {
         const buildSteps = []
         const level = () => (buildSteps.length ? buildSteps[buildSteps.length-1].level+1 : 0)
         for (const step of await this.getProcessingSteps()) {
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            prePhase && buildSteps.push(...await prePhase.buildNodes.getProcessingSteps(level()))
            buildSteps.push(...await phase.buildNodes.getProcessingSteps(level()))
            postPhase && buildSteps.push(...await postPhase.buildNodes.getProcessingSteps(level()))
         }
         return buildSteps
      }

      /** @return {Promise<void>} */
      async build() {
         for (const step of await this.getProcessingSteps()) {
            this.logStep(step)
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            prePhase && await prePhase.buildNodes.build()
            await phase.buildNodes.build()
            postPhase && await postPhase.buildNodes.build()
         }
      }

      /** @param {TProcessingStep<BuildPhase>} step */
      logStep(step) { console.log(`\nPhase ::${step.node.name}:: ${step.needUpdate?"build":"skip"}`) }
   }

   class BuildPhases extends DependencyGraph {
      /** @type {BuildPhase} */
      defaultPhase
      /** @type {Map<string,BuildPhase>} */
      phases

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      /** @param {JSPO_Phases_Definition} phasesDefinition */
      constructor(phasesDefinition) {
         super()
         /** @type {Map<string,BuildPhase>} */
         const phases = new Map()
         /** @param {string} name @param {number} i @param {string} attrName @return {BuildPhase} */
         const getPhase = (name, i, attrName) => {
            const phase = phases.get(name)
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
            if (phases.has(name))
               error(`Expect argument phasesDefinition.phases[${i}].name = »${name}« to be unique.`)
            const phase = new BuildPhase(name)
            phases.set(phase.name, phase)
         })
         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            if (phaseDefinition.dependsOnPhases && !Array.isArray(phaseDefinition.dependsOnPhases))
               error(`Expect argument phasesDefinition.phases[${i}].dependsOnPhases of type Array instead of »${typeof phaseDefinition.dependsOnPhases}«.`)
            const dependsOnPhases = (phaseDefinition.dependsOnPhases ?? []).map( (name, i2) =>
               getPhase(name,i,`dependsOnPhases[${i2}]`)
            )
            const prePhase = phaseDefinition.prePhase === undefined ? undefined : getPhase(phaseDefinition.prePhase,i,"prePhase")
            const postPhase = phaseDefinition.postPhase === undefined ? undefined : getPhase(phaseDefinition.postPhase,i,"postPhase")
            prePhase?.name === phaseDefinition.name && error(`Expect argument phasesDefinition.phases[${i}].prePhase = »${String(prePhase.name)}« to reference not itself.`)
            postPhase?.name === phaseDefinition.name && error(`Expect argument phasesDefinition.phases[${i}].postPhase = »${String(postPhase.name)}« to reference not itself.`)
            getPhase(phaseDefinition.name,i,"name").setPrePost(prePhase,postPhase).setDependency(dependsOnPhases)
         })
         this.defaultPhase = phases.get(phasesDefinition.defaultPhase) ?? error(`Expect argument phasesDefinition.defaultPhase = »${phasesDefinition.defaultPhase}« to reference an existing phase with same name.`)
         this.phases = phases
         this.validateSync()
      }

      /** @return {Promise<void>} */
      async validate() { return this.validateSync() }

      /** @return {void} */
      validateSync() {
         /** @param {undefined|BuildPhase} phase @param {BuildPhase} usedFrom */
         const checkNoDependency = (phase, usedFrom) => {
            if (phase && (phase.prePhase || phase.postPhase || phase.dependsOnPhases.length>0)) error(`Phase »${phase.name}« is not allowed to depend on other phases cause it is used from phase »${usedFrom.name}« as ${usedFrom.prePhase===phase?"prePhase":"postPhase"}.`)
         }
         DependencyGraph.validate(this.phases)
         for (const phase of this.phases.values()) {
            checkNoDependency(phase.prePhase, phase)
            checkNoDependency(phase.postPhase, phase)
         }
      }

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) {
         /** @type {any} */
         const step = new ProcessingStep(node)
         return step
      }

      /** @param {number} level @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level=0) { return DependencyGraph.getProcessingSteps(this.phases, level, BuildPhases.newProcessingStep) }

      ///////////////

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.phases.get(name) ?? error(`Phase »${String(name)}« does no exist.`) }
   }

   return new JSPO()
})()

jspo.definePhases({
   defaultPhase: "build-javascript-modules",
   phases: [
      { name: "pre-clean" },
      { name: "post-clean" },
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
