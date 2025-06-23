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
    * @typedef JSPO_Project_Definition
    * @property {string} project
    * @property {JSPO_Phase_Definition[]} phases
    *
    * @typedef JSPO_Phase_Definition
    * @property {string} phase
    * @property {string[]} [dependsOnPhases]
    * @property {{project:string,phase:string}[]} [dependsOnProjects]
    * @property {string} [prePhase]
    * @property {string} [postPhase]
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
      /** @type {BuildProjects} */
      #projects
      /** @type {undefined|BuildProject} */
      #activeProject
      /** @type {undefined|BuildPhase} */
      #activePhase

      constructor() {
         this.#files = new Files()
         this.#projects = new BuildProjects()
         this.#activeProject = undefined
         this.#activePhase = undefined
      }

      /** @return {BuildProject} */
      get activeProject() { return this.#activeProject ?? error(`No active project, use switchProject to set one after adding it with addProject.`) }
      /** @return {BuildPhase} */
      get activePhase() { return this.#activePhase ?? error(`No active phase, use switchPhase to set one after switchProject.`) }
      /** @type {Files} Allows to manipulate system files. */
      get files() { return this.#files }
      /** @type {Map<string,BuildPhase>} */
      get phases() { return this.activeProject.projectPhases.phases }
      /** @type {string[]} */
      get projectNames() { return [...this.#projects.projects.keys()] }
      /** @type {BuildProjects} */
      get projects() { return this.#projects }
      /** @return {SharedPhases} */
      get sharedPhases() { return this.#projects.sharedPhases }
      /** @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps() { return this.activePhase.buildSteps() }
      /** @return {Promise<void>} */
      async build() { return this.activePhase.build() }
      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.activeProject.phase(name) }
      /** @param {string} name @return {BuildProject} */
      project(name) { return this.#projects.project(name) }

      /** @param {JSPO_BuildRule_Definition} build */
      addBuild(build) { this.activePhase.addBuild(build) }

      /** @param {JSPO_Project_Definition} project */
      addProject(project) { this.#projects.addProject(project) }

      /** @param {string} name @param {string} [phase] */
      switchProject(name, phase) {
         this.#activeProject = this.project(name)
         this.switchPhase(phase)
      }

      /** @param {string} [name] */
      switchPhase(name) { this.#activePhase = name === undefined ? name : this.phase(name) }

      async validateProjects() { return this.#projects.validate() }

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

      /** @return {this[]} */
      get typedDependentOn() {
         /** @type {any} */
         const untyped = this.dependentOn
         return untyped
      }

      /** @return {string} */
      get canonicalName() { return this.path }

      /** @param {DependentNode[]} dependentOn @param {{ new(...args:any[]): DependentNode}} nodeType */
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

      /**
       * @template {DependentNode} T
       * @param {T} node
       * @return {TProcessingStep<T>}
       */
      static create(node) {
         /** @type {any} */
         const step = new ProcessingStep(node)
         return step
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
         const node2 = newNode(path)
         return nodes.set(path, node2), node2
      }

      /**
       * @template {DependentNode} T
       * @param {T} node
       * @return {Map<string,T>}
       */
      static getClosure(node) {
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

      /** @param {Map<string,DependentNode>} nodes @param {Map<string,DependentNode>} subset */
      static validateSubset(nodes, subset) {
         for (const node of subset.values())
            if (nodes.get(node.path) !== node)
               error(`Node »${node.path}« in subset not part of whole set (internal error).`)
         this.validateAllNodesContained(nodes,subset)
         this.validateNoCycles(nodes)
      }

      /** @param {Map<string,DependentNode>} nodes @param {Map<string,DependentNode>} [subset] */
      static validateAllNodesContained(nodes, subset) {
         for (const node of (subset?.values() ?? nodes.values())) {
            node.dependentOn.forEach( (node2, i) => {
               if (nodes.get(node2.path) !== node2) {
                  error(`Node »${node.path}«.dependentOn[${i}] references uncontained node ${node2.path}.`)
               }
            })
         }
      }

      /** @param {Map<string,DependentNode>} nodes */
      static validateNoCycles(nodes) { this.getProcessingSteps(nodes, 0, ProcessingStep.create) }

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

      /** @param {TProcessingStep<DependentNode>[]} steps */
      static cycleError(steps) {
         /** @param {TProcessingStep<DependentNode>} step @return {undefined|TProcessingStep<DependentNode>[]} */
         const findCycle = (step) => {
            step.unprocessedDependentOn = -1 // -1: in-processing, 0: processed, 1..: wait-for-processing
            for (const follow of step.reverseDependentOn) {
               if (follow.unprocessedDependentOn === -1)
                  return [follow]
               if (follow.unprocessedDependentOn > 0) {
                  const cycle = findCycle(follow)
                  if (cycle) return [...cycle,follow]
               }
            }
            step.unprocessedDependentOn = 0
         }
         for (const step of steps) {
            if (step.unprocessedDependentOn > 0) {
               const cycle = findCycle(step)
               if (cycle) {
                  error(`Nodes »${cycle.map(s=>s.node.constructor.name+":"+s.node.canonicalName).join()}« ${cycle.length===1?"is":"are"} part of a dependency cycle.`)
               }
            }
         }
         error(`Node »???« is part of a dependency cycle.`)
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
            if (nextSteps.length === 0)
               this.cycleError(steps)
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

   /**
    * Action implemented as callback called from an executed build step.
    */
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

   /**
    * Describes a single potential build step.
    * A build step wraps a BuildNode and is of type TProcessingStep<BuildNode>.
    */
   class BuildNode extends DependentNode {
      /** @type {undefined|BuildRule} */
      buildRule
      /** @type {string} Path of node which referenced this node the first time. */
      firstReferencedFrom
      /** @param {string} path @param {string} firstReferencedFrom */
      constructor(path, firstReferencedFrom) {
         super(path)
         this.firstReferencedFrom = firstReferencedFrom
      }
      /** @return {boolean}  */
      get isVirtual() { return error("get isVirtual not implemented in subtype.") }
      /** @return {string}  */
      get type() { return error("get type not implemented in subtype.") }
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

   /**
    * TODO:
    */
   class BuildNodes extends DependencyGraph {
      /** @type {Map<string,BuildNode>} Stores all CommandNodes/FileNodes indexed by name/path which must be build. */
      buildNodes = new Map()
      /** @type {boolean} */
      validated = false

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      constructor() { super() }

      async validate() {
         if (this.validated) return
         DependencyGraph.validate(this.buildNodes)
         for (const node of this.buildNodes.values())
            if (!node.buildRule && (node.isVirtual || !(await Bun.file(node.path).exists())))
               error(`Undefined build rule for ${node.type} »${node.path}«.`)
         this.validated = true
      }

      /** @param {number} level @return {Promise<TProcessingStep<BuildNode>[]>} */
      async getProcessingSteps(level=0) {
         this.validate()
         const updateNodes = new Set()
         /** @type {(node:BuildNode)=>TProcessingStep<BuildNode>} */
         const newProcessingStep = ProcessingStep.create
         const steps = DependencyGraph.getProcessingSteps(this.buildNodes,level,newProcessingStep)
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
         const isCommand = "command" in build
         const path = isCommand ? build.command : build.file
         typeof path === "string" || error(`Expect argument build.${isCommand?"command":"file"} of type string instead of »${typeof path}«.`)
         const dependsOnCommands = build.dependsOnCommands ?? []
         const dependsOnFiles = build.dependsOnFiles ?? []
         const action = build.action
         typeof action === "function" || error(`Expect argument build.action of type function instead of »${typeof action}«.`)
         /** @param {BuildNode} node */
         const firstDefinedErrorMsg = (node) => node.path === node.firstReferencedFrom ? `but node »${node.path}« is defined as ${node.type} node.` : `but node »${node.path}« is referenced as a ${node.type} node from node »${node.firstReferencedFrom}«.`
         {
            const foundNode = this.buildNodes.get(path)
            if (foundNode) {
               foundNode.buildRule && error(`Expect argument build.${isCommand?"command":"file"} = »${path}« to be unique.`)
               isCommand !== (foundNode instanceof CommandNode) && error(`Expect argument build.${isCommand?"command":"file"} = »${path}« to define a ${isCommand?"command":"file"} node ${firstDefinedErrorMsg(foundNode)}`)
            }
         }
         /** @param {string[]} dependsOn @param {string} argName @param {boolean} isCommand */
         const checkType = (dependsOn, argName, isCommand) => {
            Array.isArray(dependsOn) || error(`Expect argument build.${argName} of type Array instead of »${typeof dependsOn}«.`)
            dependsOn.forEach( (path,i) => (typeof path === "string") || error(`Expect argument build.${argName}[${i}] of type string instead of »${typeof path}«.`))
            dependsOn.forEach( (path,i) => { const foundNode = this.buildNodes.get(path); foundNode && isCommand !== (foundNode instanceof CommandNode) && error(`Expect argument build.${argName}[${i}] = »${path}« to reference a ${isCommand?"command":"file"} node ${firstDefinedErrorMsg(foundNode)}`) })
         }
         checkType(dependsOnCommands,"dependsOnCommands",true)
         checkType(dependsOnFiles,"dependsOnFiles",false)
         const firstReferencedFrom = path
         /** @param {string} path @param {boolean} isCommand */
         const getNode = (path, isCommand) => {
            return DependencyGraph.ensureNode(path, this.buildNodes, () => isCommand ? new CommandNode(path,firstReferencedFrom) : new FileNode(path,firstReferencedFrom))
         }
         const buildNode = getNode(path, isCommand)
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

   /**
    * TODO:
    */
   class BuildPhase extends DependentNode {
      /** @type {BuildNodes} */
      buildNodes
      /** @type {undefined|BuildPhase} */
      prePhase
      /** @type {undefined|BuildPhase} */
      postPhase
      /** @type {BuildPhase} */
      createdFrom
      /** @type {string} */
      name
      /** @type {string} */
      projectName
      /** @type {undefined|BuildProject} */
      definedWithinProject

      /////////////////////////////
      // Overwrite DependentNode //
      /////////////////////////////

      /** @param {string} projectName @param {string} phaseName @param {BuildPhase} [createdFrom] */
      constructor(projectName, phaseName, createdFrom) {
         super(BuildPhase.path(projectName,phaseName))
         this.buildNodes = new BuildNodes()
         this.createdFrom = createdFrom ?? this
         this.name = phaseName
         this.projectName = projectName
      }

      /**
       * @param {BuildPhase[]} dependentOn
       */
      setDependency(dependentOn) { super.setDependency(dependentOn, BuildPhase) }

      ///////////

      /** @param {string} project @param {string} phase */
      static path(project, phase) {
         typeof project === "string" || error(`Expect argument project of type string instead of ${typeof project}.`)
         typeof phase === "string" || error(`Expect argument phase of type string instead of ${typeof phase}.`)
         return JSON.stringify(project)+","+JSON.stringify(phase)
      }

      get canonicalName() { return `project:${this.projectName},phase:${this.name}`}

      /** @type {boolean} */
      get isDefined() { return this.definedWithinProject !== undefined }

      /** @return {Promise<void>} */
      async validate() { return this.buildNodes.validate() }

      ///////////

      /** @param {BuildProject} project @return {BuildPhase} */
      setDefinedWithinProject(project) {
         if (this.definedWithinProject && project !== this.definedWithinProject) error(`Phase »${this.name}« is defined from project »${this.definedWithinProject.name}« and also »${project.name}«.`)
         this.definedWithinProject = project
         return this
      }

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

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) { return ProcessingStep.create(node) }

      /** @param {number} level @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level=0) {
         /** @type {Map<string,BuildPhase>} */
         const phases = DependencyGraph.getClosure(this)
         /** @type {TProcessingStep<BuildPhase>[]} */
         const steps = DependencyGraph.getProcessingSteps(phases, level, BuildPhase.newProcessingStep)
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
      logStep(step) { console.log(`\nProject-Phase ::${step.node.projectName}::${step.node.name}::`) }
   }

   class ProjectPhases {
      /** @type {SharedPhases} */
      sharedPhases
      /** @type {Map<string,BuildPhase>} Maps name (not full path) to BuildPhase. */
      phases
      /** @type {BuildProject} */
      project

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      /** @param {BuildProject} project @param {JSPO_Project_Definition} phasesDefinition @param {SharedPhases} sharedPhases */
      constructor(project, phasesDefinition, sharedPhases) {
         /** @type {Map<string,BuildPhase>} */
         const phases = new Map()
         /** @param {string} name @param {number} i @param {string} attrName @return {BuildPhase} */
         const getPhase = (name, i, attrName) => phases.get(name) ?? error(`Undefined phase »${name}« referenced from phasesDefinition.phases[${i}].${attrName}.`)
         if (phasesDefinition === null || typeof phasesDefinition !== "object")
            error(`Expect argument phasesDefinition of type object instead of »${phasesDefinition === null ? "null" : typeof phasesDefinition}«.`)
         if (!Array.isArray(phasesDefinition.phases))
            error(`Expect argument phasesDefinition.phases of type Array instead of »${typeof phasesDefinition.phases}«.`)
         if (phasesDefinition.phases.length === 0)
            error(`Expect argument phasesDefinition.phases not empty.`)
         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            const phase = phaseDefinition.phase
            if (typeof phase !== "string")
               error(`Expect argument phasesDefinition.phases[${i}].phase of type string instead of »${typeof phase}«.`)
            if (phases.has(phase))
               error(`Expect argument phasesDefinition.phases[${i}].phase = »${phase}« to be unique.`)
            if (phaseDefinition.prePhase !== undefined) {
               typeof phaseDefinition.prePhase === "string" || error(`Expect argument phasesDefinition.phases[${i}].prePhase of type string instead of »${typeof phaseDefinition.prePhase}«.`)
               phaseDefinition.prePhase !== phase || error(`Expect argument phasesDefinition.phases[${i}].prePhase = »${String(phase)}« to reference not itself.`)
            }
            if (phaseDefinition.postPhase !== undefined) {
               typeof phaseDefinition.postPhase === "string" || error(`Expect argument phasesDefinition.phases[${i}].postPhase of type string instead of »${typeof phaseDefinition.postPhase}«.`)
               phaseDefinition.postPhase !== phase || error(`Expect argument phasesDefinition.phases[${i}].postPhase = »${String(phase)}« to reference not itself.`)
            }
            if (phaseDefinition.dependsOnPhases) {
               Array.isArray(phaseDefinition.dependsOnPhases) || error(`Expect argument phasesDefinition.phases[${i}].dependsOnPhases of type Array instead of »${typeof phaseDefinition.dependsOnPhases}«.`)
               phaseDefinition.dependsOnPhases.forEach( (name, i2) => typeof name === "string" || error(`Expect argument phasesDefinition.phases[${i}].dependsOnPhases[${i2}] of type string instead of ${typeof name}.`))
            }
            if (phaseDefinition.dependsOnProjects) {
               Array.isArray(phaseDefinition.dependsOnProjects) || error(`Expect argument phasesDefinition.phases[${i}].dependsOnProjects of type Array instead of »${typeof phaseDefinition.dependsOnProjects}«.`)
               phaseDefinition.dependsOnProjects.forEach( (entry, i2) => typeof entry === "object" || error(`Expect argument phasesDefinition.phases[${i}].dependsOnProjects[${i2}] of type object instead of ${typeof entry}.`))
               phaseDefinition.dependsOnProjects.forEach( ({project,phase}, i2) =>
                  (typeof project === "string" || error(`Expect argument phasesDefinition.phases[${i}].dependsOnProjects[${i2}].project of type string instead of ${typeof project}.`))
                  && (typeof phase === "string" || error(`Expect argument phasesDefinition.phases[${i}].dependsOnProjects[${i2}].phase of type string instead of ${typeof phase}.`))
               )
            }
            phases.set(phase, sharedPhases.ensureNode(project.name, phase).setDefinedWithinProject(project))
         })
         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            const createdFrom = sharedPhases.ensureNode(project.name, phaseDefinition.phase)
            const dependsOnPhases = (phaseDefinition.dependsOnPhases ?? []).map( (name, i2) => getPhase(name,i,`dependsOnPhases[${i2}]`))
            const dependsOnProjects = (phaseDefinition.dependsOnProjects ?? []).map( ({project,phase}, i2) => sharedPhases.ensureNode(project, phase, createdFrom))
            const prePhase = phaseDefinition.prePhase === undefined ? undefined : getPhase(phaseDefinition.prePhase,i,"prePhase")
            const postPhase = phaseDefinition.postPhase === undefined ? undefined : getPhase(phaseDefinition.postPhase,i,"postPhase")
            dependsOnPhases.forEach( (phase) => sharedPhases.ensureNode(project.name, phase.name, createdFrom))
            getPhase(phaseDefinition.phase,i,"phase").setPrePost(prePhase,postPhase).setDependency(dependsOnProjects.concat(dependsOnPhases))
         })
         this.sharedPhases = sharedPhases
         this.project = project
         this.phases = phases
         this.validateSync()
      }

      /** @return {void} */
      validateSync() {
         /** @param {undefined|BuildPhase} phase @param {BuildPhase} usedFrom */
         const checkNoDependency = (phase, usedFrom) => {
            if (phase && (phase.prePhase || phase.postPhase || phase.dependentOn.length>0)) error(`Expect phase »${phase.name}« to depend on no other phases cause of being used as ${usedFrom.prePhase===phase?"prePhase":"postPhase"} from phase »${usedFrom.name}«.`)
         }
         DependencyGraph.validateSubset(this.sharedPhases.nodes,this.phases)
         for (const phase of this.phases.values()) {
            checkNoDependency(phase.prePhase, phase)
            checkNoDependency(phase.postPhase, phase)
         }
      }

      async validate() {
         for (const phase of this.phases.values())
            try { await phase.validate() }
            catch (e) { error(`Validation of »${phase.canonicalName}« failed. ${e}`) }
      }

      ///////////

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.phases.get(name) ?? error(`Phase »${String(name)}« does not exist.`) }
   }

   class BuildProject {
      /** @type {string} */
      name
      /** @type {ProjectPhases} */
      projectPhases

      /** @param {string} name @param {JSPO_Project_Definition} phasesDefinition @param {SharedPhases} sharedPhases */
      constructor(name, phasesDefinition, sharedPhases) {
         this.name = name
         this.projectPhases = new ProjectPhases(this, phasesDefinition, sharedPhases)
      }

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.projectPhases.phase(name) }

      /** @return {Promise<void>} */
      async validate() { return this.projectPhases.validate() }

   }

   class BuildProjects {
      /** @type {SharedPhases} */
      sharedPhases
      /** @type {Map<string,BuildProject>} */
      projects

      constructor() {
         this.sharedPhases = new SharedPhases(this)
         this.projects = new Map()
      }

      /** @return {Promise<void>} */
      async validate() {
         for (const project of this.projects.values())
            await project.validate()
         await this.sharedPhases.validate()
      }

      ///////////

      /** @param {string} name @return {BuildProject} */
      project(name) { return this.projects.get(name) || error(`Project »${String(name)}« is not defined.`) }

      /** @param {string} name @return {boolean} */
      has(name) { return this.projects.has(name) }

      ///////////

      /** @param {JSPO_Project_Definition} projectDefinition */
      addProject(projectDefinition) {
         const projectName = projectDefinition.project
         typeof projectName === "string" || error(`Expect argument projectDefinition.project of type string instead of »${typeof projectName}«.`)
         this.projects.get(projectName) && error(`Expect argument projectDefinition.project = »${projectName}« to be unique.`)
         const newProject = new BuildProject(projectName, projectDefinition, this.sharedPhases)
         this.projects.set(projectName, newProject)
      }
   }

   class SharedPhases extends DependencyGraph {
      /** @type {Map<string,BuildPhase>}  */
      nodes
      /** @type {BuildProjects} */
      projects

      /** @type {BuildProjects} projects */
      constructor(projects) {
         super()
         this.nodes = new Map()
         this.projects = projects
      }

      /** @return {Promise<void>} */
      async validate() {
         DependencyGraph.validate(this.nodes)
         this.validateAllPhasesDefined()
      }

      /** @return {void} */
      validateAllPhasesDefined() {
         for (const node of this.nodes.values())
            node.isDefined || error(`Undefined »${this.projects.has(node.projectName)?node.canonicalName:"project:"+node.projectName}« referenced from »${node.createdFrom.canonicalName}«.`)
      }

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) { return ProcessingStep.create(node) }

      /** @param {number} level @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level=0) { return DependencyGraph.getProcessingSteps(this.nodes, level, SharedPhases.newProcessingStep) }

      ///////////

      /** @param {string} project @param {string} phase @param {BuildPhase} [createdFrom] */
      ensureNode(project, phase, createdFrom) { return DependencyGraph.ensureNode(BuildPhase.path(project,phase), this.nodes, () => new BuildPhase(project,phase,createdFrom)) }

   }

   return new JSPO()
})()

jspo.addProject({
   project: "build-javascript",
   phases: [ { phase: "build" } ]
})

jspo.project("build-javascript").phase("build").addBuild( { command:"build javascript files", action: async ({path}) => console.log(path) })

jspo.addProject({
   project: "default",
   phases: [
      { phase: "pre-clean" },
      { phase: "post-clean" },
      { phase: "xxx", prePhase: "pre-clean", postPhase: "post-clean" },
      { phase: "clean", prePhase: "pre-clean", postPhase: "post-clean" },
      { phase: "build-javascript-modules", dependsOnPhases: ["clean","xxx"], dependsOnProjects: [{project:"build-javascript",phase:"build"}] },
   ]
})

jspo.switchProject("default", "build-javascript-modules")

// project: "default" && phase: "build-javascript-modules"
jspo.addBuild({ file:"test.mjs", dependsOnFiles:["html-snippets/test.js"], action: async () => {
   console.log("Build test.mjs")
   await jspo.files.overwrite("test.mjs",
      jspo.appendESModuleExport(
            await jspo.files.loadString("html-snippets/test.js"),
            [ "TEST", "RUN_TEST", "SUB_TEST", "END_TEST", "RESET_TEST", "NEW_TEST" ]
      ),
   )
}})
jspo.addBuild({ command:"test.mjs postprocessor", dependsOnFiles:["test.mjs"], action:async ()=>console.log("test.mjs postprocessor") })

jspo.switchPhase("xxx")

// project: "default" && phase: "xxx"
jspo.addBuild({ file:"xxx1", dependsOnFiles:["xxx2","xxx3"], action: async ()=>console.log("build xxx1") })
jspo.addBuild({ file:"xxx10", dependsOnFiles:["xxx1","xxx3"], action: async ()=>console.log("build xxx10") })

const cleanPhase = jspo.phase("clean")
cleanPhase.addBuild({ command:"remove-files", dependsOnCommands:[], action:async ()=>console.log("remove files") })
cleanPhase.addBuild({ command:"remove-dirs", dependsOnCommands:["remove-files"], action:async ()=>console.log("remove directories") })
jspo.phase("pre-clean").addBuild({ command:"pre-clean", action:async ()=>console.log("pre-clean commands") })
jspo.phase("post-clean").addBuild({ command:"post-clean", action:async ()=>console.log("post-clean commands") })
jspo.phase("xxx").addBuild({ command:"xxx", action:async ()=>console.log("xxx commands") })

await jspo.validateProjects()

console.log("=== cleanPhase.build() ===")

await cleanPhase.build()

console.log("\n=== jspo.build() ===")

jspo.switchPhase("build-javascript-modules")

await jspo.buildSteps()
   // TODO: .then( steps => console.log(steps))
   .then( () => jspo.build())
   .catch( e => console.log("error",e))

// validation cycle

jspo.addProject({
   project: "project-with-cycles",
   phases: [ { phase: "build" } ]
})

jspo.switchProject("project-with-cycles", "build")
jspo.addBuild({ command:"c1", dependsOnCommands:["c2"], action: async ()=>console.log("build c1") })
jspo.addBuild({ command:"c2", dependsOnCommands:["c3"], action: async ()=>console.log("build c2") })
jspo.addBuild({ command:"c3", dependsOnCommands:["c4"], action: async ()=>console.log("build c3") })
jspo.addBuild({ command:"c4", dependsOnCommands:["c1"], action: async ()=>console.log("build c4") })

console.log("\n=== jspo.validateProjects() ===")

await jspo.validateProjects().catch(e => {
   console.log("\n"+e.toString())
})
