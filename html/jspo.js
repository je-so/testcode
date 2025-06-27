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
    * @property {JSPO_BuildRule_Definition[]} [builds]
    *
    * @typedef JSPO_BuildAction_Arg
    * @property {string} path
    * @property {string[]} dependsOnCommands
    * @property {string[]} dependsOnFiles
    * @property {ErrorContext} [ec]
    *
    * @typedef {(arg:JSPO_BuildAction_Arg)=>Promise<void>} JSPO_BuildAction
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
    *
    * @typedef JSPO_ErrorContext_Arg
    * @property {string} fct
    * @property {(number|string)[]} [accesspath]
    * @property {undefined|string} [project]
    * @property {undefined|string} [phase]
    * @property {undefined|BuildNode} [build]
    *
    */

   class ErrorContext {
      /** @param {JSPO_ErrorContext_Arg} arg */
      constructor(arg) {
         this.fct = arg.fct
         this.accesspath = arg.accesspath ?? []
         this.project = arg.project
         this.phase = arg.phase
         this.build = arg.build
      }

      /** @param {BuildNode} build */
      newBuild(build) { return new ErrorContext({...this,build}) }
      /** @param {string} phase */
      newPhase(phase) { return new ErrorContext({...this,phase}) }
      /** @param {string} project @param {string} [phase] */
      newProject(project, phase) { return new ErrorContext(phase?{...this,project,phase}:{...this,project}) }
      /** @param {(number|string)[]} subpath */
      newSubPath(...subpath) { return new ErrorContext({...this,accesspath:this.accesspath.concat(subpath)}) }

      /** @param {(number|string)[]} subpath @return {string} Returns access path for argument which caused an error. */
      ap(...subpath) {
         const accesspath = this.accesspath.concat(subpath)
         let path = accesspath.length ? String(accesspath[0]) : ""
         for (let i=1; i<accesspath.length; ++i)
            path += typeof accesspath[i] === "number" ? "["+accesspath[i]+"]" : "."+accesspath[i]
         return path
      }
      errcontext() {
         const forProject = this.project === undefined ? "" : ` for project ${JSON.stringify(this.project)}`
                            + (this.phase === undefined ? "" : `, phase ${JSON.stringify(this.phase)}`)
                            + (this.build === undefined ? "" : `, build ${this.build.type} ${JSON.stringify(this.build.path)}`)
         return `${this.fct} failed${forProject}`
      }
      /** @param {string} msg */
      log(msg) {
         console.group("error:",this.errcontext())
         console.log(msg)
         console.trace()
         console.groupEnd()
      }

   }

   /** @param {string} msg Message thrown as an Error. @param {ErrorContext} context @param {Error} [cause] */
   const error = (msg,context,cause) => {
      if (context)
         msg = `${context.errcontext()}\n${msg}`
      else {
         const stack = Error().stack?.match(/.*\n[^\n]* error [^\n]*\n([^\n]*)/)
         msg = stack ? `Function ${stack[1].trim()} failed with internal error\n${msg}` : msg
      }
      msg = msg.replaceAll("\n","\n   ")
      throw (cause ? Error(msg,{cause}) : Error(msg))
   }

   /**
    * The interface to the "JavaScript Projects Organizer" accessed with variable jspo.
    * A single jspo instance manages one or more projects.
    * A project consists of one or more build phases and every phase has one or more build rules
    * stored in build nodes.
    *
    * A build node is either a {@link CommandNode} or {@link FileNode}.
    * Commands are virtual and are always processed if they have no dependencies.
    * If they have dependencies then they are only processed if a dependency is processed.
    * The build rule for file is only executed if the file does not exist or if one of its
    * dependencies is newer or is also processed (in case of a command).
    *
    * Build nodes could depend on each other but only if they are all part of the same build phase.
    * Build phases could also depend on each other. They could depend on phases which are part of other projects.
    *
    */
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
      get activeProject() { return this.#activeProject ?? error(`No active project, use switchProject to set one after adding it with addProject.`,new ErrorContext({fct:"activeProject()"})) }
      /** @return {BuildPhase} */
      get activePhase() { return this.#activePhase ?? error(`No active phase, use switchPhase or switchProject to set one.`,new ErrorContext({fct:"activePhase()"})) }
      /** @type {Files} Allows to manipulate system files. */
      get files() { return this.#files }
      /** @type {Map<string,BuildPhase>} */
      get phases() { return this.activeProject.projectPhases.phases }
      /** @type {string[]} */
      get projectNames() { return [...this.#projects.projects.keys()] }
      /** @type {BuildProjects} */
      get projects() { return this.#projects }
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

      /** @param {JSPO_Phase_Definition} phase */
      addPhase(phase) { this.activeProject.addPhase(phase) }

      /** @param {JSPO_Project_Definition} project */
      addProject(project) { this.#projects.addProject(project) }

      /** @param {string} [name] */
      switchPhase(name) { this.#activePhase = name === undefined ? name : this.phase(name) }

      /** @param {string} name @param {string} [phase] */
      switchProject(name, phase) {
         this.#activeProject = this.project(name)
         this.switchPhase(phase)
      }

      async validateProjects() { return this.#projects.validate(new ErrorContext({fct:"validateProjects()"})) }

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
       * @param {ErrorContext} [ec]
       * @return {Promise<string>}
       * */
      async loadString(path, ec) {
         const file = Bun.file(path)
         if (await file.exists()) {
            return file.text()
         }
         return error(`File ${JSON.stringify(path)} does not exist.`,ec ?? new ErrorContext({fct:"loadString(path)"}))
      }

      /**
       * @param {string} path
       * @param {string} content
       * @param {ErrorContext} ec
       * @return {Promise<number>} The number of bytes written to disk.
       */
      async write(path, content, ec) {
         const file = Bun.file(path)
         await file.exists() && error(`File ${JSON.stringify(path)} exists use overwrite instead.`,ec ?? new ErrorContext({fct:"write(path)"}))
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
       * @param {ErrorContext} [ec]
       * @return {Promise<void>}
       */
      async touch(path, ec) {
         return this.exists(path).then(exists => exists ? fs.promises.utimes(path, Date.now()/1000, Date.now()/1000) : Bun.write(path, ""))
                  .catch(e => error(`Accessing file ${JSON.stringify(path)} failed with system failure:`,ec??new ErrorContext({fct:"touch(path)"}),e))
      }

      /** @param {string} path @return {Promise<number>} Time of last modification of file in milliseconds since Epoch. */
      async mtime(path) {
         if (await Bun.file(path).exists())
            return fs.statSync(path).mtimeMs
         return -1
      }

      /** @param {string} path @return {Promise<boolean>} */
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
         typeof path === "string" || error(`Expect argument path of type string instead of ${typeof path} (internal error).`,new ErrorContext({fct:"new DependentNode()"}))
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

      /** @param {DependentNode[]} dependentOn @param {ErrorContext} ec @param {{ new(...args:any[]): DependentNode}} nodeType */
      setDependency(dependentOn, ec, nodeType) {
         /** @param {any} node @param {number} i */
         const checkType = (node,i) => { if (!(node instanceof nodeType)) error(`Expect argument dependentOn[${i}] of type ${nodeType.name} instead of ${node?.constructor?.name ?? typeof node} (internal error).`,ec) }
         Array.isArray(dependentOn) || error(`Expect argument dependentOn of type Array instead of ${typeof dependentOn} (internal error).`,ec)
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

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) { error(`validate() not overwritten in subtype (internal error).`,ec) }

      /** @param {number} level @param {ErrorContext} ec @return {Promise<ProcessingStep[]>} */
      async getProcessingSteps(level, ec) { return error(`getProcessSteps() not overwritten in subtype (internal error).`,ec) }

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

      /** @param {Map<string,DependentNode>} nodes @param {ErrorContext} ec @return {void} */
      static validate(nodes, ec) {
         this.validateAllNodesContained(nodes, nodes, ec)
         this.validateNoCycles(nodes, ec)
      }

      /** @param {Map<string,DependentNode>} nodes @param {Map<string,DependentNode>} subset @param {ErrorContext} ec */
      static validateSubset(nodes, subset, ec) {
         for (const node of subset.values())
            nodes.get(node.path) === node || error(`Node »${node.path}« in subset is not part of whole set (internal error).`,ec)
         this.validateAllNodesContained(nodes,subset,ec)
         this.validateNoCycles(nodes,ec)
      }

      /** @param {Map<string,DependentNode>} nodes @param {Map<string,DependentNode>} subset @param {ErrorContext} ec */
      static validateAllNodesContained(nodes, subset, ec) {
         for (const node of subset.values())
            node.dependentOn.forEach((node2, i) => nodes.get(node2.path) === node2 || error(`Node »${node.path}«.dependentOn[${i}] references uncontained node ${node2.path} (internal error).`,ec))
      }

      /** @param {Map<string,DependentNode>} nodes @param {ErrorContext} ec */
      static validateNoCycles(nodes, ec) { this.getProcessingSteps(nodes, 0, ProcessingStep.create, ec) }

      ///////////////////////////////////
      // Layout Nodes in Process Order //
      ///////////////////////////////////

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes
       * @param {number} level
       * @param {(node:DependentNode)=>TProcessingStep<T>} newProcessingStep
       * @param {ErrorContext} ec
       * @return {TProcessingStep<T>[]}
       */
      static getProcessingSteps(nodes, level, newProcessingStep, ec) {
         return this.orderSteps(this.wrapNodes(nodes, newProcessingStep),level,ec)
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

      /** @param {TProcessingStep<DependentNode>[]} steps @param {ErrorContext} ec */
      static cycleError(steps, ec) {
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
               cycle && error(`Node${cycle.length===1?"":"s"} »${cycle.map(s=>`${s.node.constructor.name}:"${s.node.canonicalName}"`).join(", ")}« ${cycle.length===1?"is":"are"} part of a dependency cycle.`,ec)
            }
         }
         error(`Dependency cycle detected.`,ec)
      }

      /**
       * @template {DependentNode} T
       * @param {TProcessingStep<T>[]} steps
       * @param {number} level
       * @param {ErrorContext} ec
       * @return {TProcessingStep<T>[]}
       */
      static orderSteps(steps, level, ec) {
         if (typeof level !== "number") level = 0
         const orderedSteps = []
         let nextSteps = steps.filter( step => step.unprocessedDependentOn === 0)
         while (orderedSteps.length < steps.length) {
            if (nextSteps.length === 0)
               this.cycleError(steps,ec)
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
      /** @param {ErrorContext} ec */
      async call(ec) { return this.action({...this.arg,ec}) }
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
      get isDefined() { return this.buildRule !== undefined }
      /** @return {boolean}  */
      get isVirtual() { return error("get isVirtual not implemented in subtype (internal error).",new ErrorContext({fct:"isVirtual()"})) }
      /** @return {string}  */
      get type() { return error("get type not implemented in subtype (internal error).",new ErrorContext({fct:"type()"})) }
      /** @return {Promise<undefined|number>}  */
      async mtime() { return error("mtime not implemented in subtype (internal error).",new ErrorContext({fct:"mtime()"})) }
      /** @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(updateNodes) { return error("needsUpdate not implemented in subtype (internal error).",new ErrorContext({fct:"needsUpdate()"})) }

      /** @param {BuildRule} buildRule */
      setBuildRule(buildRule) { this.buildRule = buildRule }
      /** @param {BuildNode[]} dependentOn @param {ErrorContext} ec */
      setDependency(dependentOn, ec) { super.setDependency(dependentOn, ec, BuildNode) }
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
    * A set of build steps which could depend on each other.
    * Cyclic dependencies are detected and reported as error.
    */
   class BuildNodes extends DependencyGraph {
      /** @type {Map<string,BuildNode>} Stores all CommandNodes/FileNodes indexed by name/path which must be build. */
      buildNodes = new Map()
      /** @type {boolean} */
      validated = false
      /** @type {BuildPhase} owner of this object */
      phase

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      /** @param {BuildPhase} phase */
      constructor(phase) {
         super()
         this.phase = phase
      }

      /** @param {ErrorContext} ec */
      async validate(ec) {
         if (this.validated) return
         ec = ec?.newPhase(this.phase.name) ?? new ErrorContext({fct:"validate()",project:this.phase.projectName,phase:this.phase.name})
         DependencyGraph.validate(this.buildNodes,ec)
         for (const node of this.buildNodes.values())
            if (!node.buildRule && (node.isVirtual || !(await jspo.files.exists(node.path))))
               error(`Undefined build rule for ${node.type} ${JSON.stringify(node.canonicalName)}.`,ec.newBuild(node))
         this.validated = true
      }

      /** @param {number} level @param {ErrorContext} ec @return {Promise<TProcessingStep<BuildNode>[]>} */
      async getProcessingSteps(level, ec) {
         this.validate(ec)
         const updateNodes = new Set()
         /** @type {(node:BuildNode)=>TProcessingStep<BuildNode>} */
         const newProcessingStep = ProcessingStep.create
         const steps = DependencyGraph.getProcessingSteps(this.buildNodes,level,newProcessingStep,ec)
         for (const step of steps) {
            if (step.needUpdate) {
               step.needUpdate = await step.node.needsUpdate(updateNodes)
               if (step.needUpdate) updateNodes.add(step.node)
            }
         }
         return steps
      }

      ///////////

      /** @param {JSPO_BuildRule_Definition} build @param {ErrorContext} [ec] */
      addBuild(build, ec) {
         const getEC = () => ec ?? new ErrorContext({fct:"addBuild(p)",accesspath:["p"],project:this.phase.projectName,phase:this.phase.name})
         typeof build === "object" || error(`Expect ${getEC().ap()} of type object instead of »${typeof build}«.`,getEC())
         this.validated = false
         const isCommand = "command" in build
         const isFile = "file" in build
         isCommand || isFile || error(`Expect ${getEC().ap("command")} or ${getEC().ap("file")} to be defined.`,getEC())
         const path = isCommand ? build.command : build.file
         typeof path === "string" || error(`Expect ${getEC().ap(isCommand?"command":"file")} of type string instead of »${typeof path}«.`,getEC())
         const dependsOnCommands = build.dependsOnCommands ?? []
         const dependsOnFiles = build.dependsOnFiles ?? []
         const action = build.action
         typeof action === "function" || error(`Expect ${getEC().ap("action")} of type function instead of »${typeof action}«.`,getEC())
         /** @param {BuildNode} node */
         const firstDefinedErrorMsg = (node) => node.isDefined ? `but node ${JSON.stringify(node.path)} is defined as ${node.type} node.` : `but node ${JSON.stringify(node.path)} is referenced as a ${node.type} node from node ${JSON.stringify(node.firstReferencedFrom)}.`
         {
            const foundNode = this.buildNodes.get(path)
            if (foundNode) {
               foundNode.buildRule && error(`Expect ${getEC().ap(isCommand?"command":"file")} = »${path}« to be unique.`,getEC())
               isCommand !== (foundNode instanceof CommandNode) && error(`Argument ${getEC().ap(isCommand?"command":"file")}=${JSON.stringify(path)} defines a ${isCommand?"command":"file"} node ${firstDefinedErrorMsg(foundNode)}`,getEC())
            }
         }
         /** @param {string[]} dependsOn @param {string} argName @param {boolean} isCommand */
         const checkType = (dependsOn, argName, isCommand) => {
            Array.isArray(dependsOn) || error(`Expect ${getEC().ap(argName)} of type Array instead of »${typeof dependsOn}«.`,getEC())
            dependsOn.forEach( (path,i) => typeof path === "string" || error(`Expect ${getEC().ap(argName,i)} of type string instead of ${typeof path}.`,getEC()))
            dependsOn.forEach( (path,i) => { const foundNode = this.buildNodes.get(path); foundNode && isCommand !== (foundNode instanceof CommandNode) && error(`Argument ${getEC().ap(argName,i)}=${JSON.stringify(path)} references a ${isCommand?"command":"file"} node ${firstDefinedErrorMsg(foundNode)}`,getEC()) })
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
         buildNode.setDependency(commandNodes.concat(fileNodes),getEC())
      }

      ///////////

      /** @return {Promise<void>} @param {ErrorContext} ec */
      async build(ec) {
         const steps = await this.getProcessingSteps(0,ec)
         for (const step of steps) {
            const node = step.node
            if (step.needUpdate && node.buildRule) {
               await node.buildRule.call(ec.newBuild(node))
               if (!node.isVirtual)
                  await jspo.files.touch(node.path,ec.newBuild(node))
            }
         }
      }

      /** @param {ErrorContext} ec @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps(ec) { return this.getProcessingSteps(0,ec) }
   }

   /**
    * A build phase is a set of build steps (implemented as BuildNodes) but which could also
    * depend on other build phases of the same project or of another project.
    * A phase supports also a pre-phase and post-phase which are additionaly processed.
    * If a pre-/port-phases are referenced from different phases they are are executed more
    * than once.
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
      /** @type {undefined|BuildProject} owner of this object */
      project

      /////////////////////////////
      // Overwrite DependentNode //
      /////////////////////////////

      /** @param {string} projectName @param {string} phaseName @param {BuildPhase} [createdFrom] */
      constructor(projectName, phaseName, createdFrom) {
         super(BuildPhase.path(projectName,phaseName))
         this.buildNodes = new BuildNodes(this)
         this.createdFrom = createdFrom ?? this
         this.name = phaseName
         this.projectName = projectName
      }

      /** @param {BuildPhase[]} dependentOn @param {ErrorContext} ec */
      setDependency(dependentOn, ec) { super.setDependency(dependentOn, ec, BuildPhase) }

      ///////////

      /** @param {string} project @param {string} phase */
      static path(project, phase) {
         typeof project === "string" || error(`Expect argument project of type string instead of ${typeof project} (internal error).`,new ErrorContext({fct:"path()"}))
         typeof phase === "string" || error(`Expect argument phase of type string instead of ${typeof phase} (internal error).`,new ErrorContext({fct:"path()"}))
         return JSON.stringify(project)+","+JSON.stringify(phase)
      }

      get canonicalName() { return `project:${this.projectName},phase:${this.name}`}

      /** @type {boolean} */
      get isDefined() { return this.project !== undefined }

      /** @type {boolean} */
      get hasDependency() { return this.prePhase !== undefined || this.postPhase !== undefined || this.dependentOn.length>0 }

      /** @param {ErrorContext} [ec] @return {Promise<void>} */
      async validate(ec) {
         ec = ec?.newPhase(this.name) ?? new ErrorContext({fct:"validate()",project:this.projectName,phase:this.name})
         this.validatePrePost(ec)
         this.validateDefined(ec)
         return this.buildNodes.validate(ec)
      }

      /** @param {ErrorContext} ec @return {void} */
      validatePrePost(ec) {
         const prePhase = this.prePhase, postPhase = this.postPhase
         prePhase?.hasDependency && error(`Expect phase ${JSON.stringify(prePhase.name)} to depend on no other phase cause of being used as prePhase from phase ${JSON.stringify(this.name)}.`,ec)
         postPhase?.hasDependency && error(`Expect phase »${JSON.stringify(postPhase.name)} to depend on no other phase cause of being used as postPhase from phase ${JSON.stringify(this.name)}.`,ec)
      }

      /** @param {ErrorContext} ec @return {void} */
      validateDefined(ec) {
         /** @param {undefined|BuildPhase} phase */
         const checkDefined = (phase) => !phase || phase.isDefined || error(`Undefined phase:${JSON.stringify(phase.canonicalName)} referenced from phase ${JSON.stringify(this.name)}.`,ec)
         checkDefined(this.prePhase)
         checkDefined(this.postPhase)
         for (const phase of this.typedDependentOn)
            checkDefined(phase)
      }

      ///////////

      /** @param {BuildProject} project @return {BuildPhase} */
      setOwner(project) {
         if (this.projectName !== project.name) error(`Phase »${this.name}« is owned by »${project.name}« instead of »${this.projectName}« (internal error).`,new ErrorContext({fct:"setOwner()",project:this.projectName,phase:this.name}))
         this.project = project
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

      /** @param {JSPO_BuildRule_Definition} build @param {ErrorContext} [ec] */
      addBuild(build, ec) {
         ec = ec?.newPhase(this.name) ?? new ErrorContext({fct:"addBuild()",project:this.projectName,phase:this.name})
         this.buildNodes.addBuild(build,ec)
      }

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) { return ProcessingStep.create(node) }

      /** @param {number} level @param {ErrorContext} ec @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level, ec) {
         /** @type {Map<string,BuildPhase>} */
         const phases = DependencyGraph.getClosure(this)
         /** @type {TProcessingStep<BuildPhase>[]} */
         const steps = DependencyGraph.getProcessingSteps(phases, level, BuildPhase.newProcessingStep, ec.newPhase(this.name))
         return steps
      }

      /** @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps() {
         const ec = new ErrorContext({fct:"buildSteps()",project:this.projectName,phase:this.name})
         const buildSteps = []
         const level = () => (buildSteps.length ? buildSteps[buildSteps.length-1].level+1 : 0)
         for (const step of await this.getProcessingSteps(0,ec)) {
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            prePhase && buildSteps.push(...await prePhase.buildNodes.getProcessingSteps(level(),ec))
            buildSteps.push(...await phase.buildNodes.getProcessingSteps(level(),ec))
            postPhase && buildSteps.push(...await postPhase.buildNodes.getProcessingSteps(level(),ec))
         }
         return buildSteps
      }

      /** @return {Promise<void>} */
      async build() {
         const ec = new ErrorContext({fct:"build()",project:this.projectName,phase:this.name})
         for (const step of await this.getProcessingSteps(0,ec)) {
            this.logStep(step)
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            prePhase && await prePhase.buildNodes.build(ec)
            await phase.buildNodes.build(ec)
            postPhase && await postPhase.buildNodes.build(ec)
         }
      }

      /** @param {TProcessingStep<BuildPhase>} step */
      logStep(step) { console.log(`\nProject-Phase ::${step.node.projectName}::${step.node.name}::`) }
   }

   /**
    * A set of build phases belonging to a project and which could depend on other phases of the same or another project.
    * Cyclic dependencies are detected and reported as error.
    */
   class ProjectPhases {
      /** @type {SharedPhases} */
      sharedPhases
      /** @type {Map<string,BuildPhase>} Maps name (not full path) to BuildPhase. */
      phases
      /** @type {BuildProject} owner of this object */
      project

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      /** @param {BuildProject} project @param {JSPO_Project_Definition} phasesDefinition @param {SharedPhases} sharedPhases @param {ErrorContext} ec */
      constructor(project, phasesDefinition, sharedPhases, ec) {
         typeof phasesDefinition === "object" || error(`Expect ${ec.ap()} of type object instead of »${typeof phasesDefinition}«.`,ec)
         Array.isArray(phasesDefinition.phases) || error(`Expect ${ec.ap("phases")} of type Array instead of »${typeof phasesDefinition.phases}«.`,ec)
         phasesDefinition.phases.length > 0 || error(`Expect ${ec.ap("phases")} not empty.`,ec)
         this.sharedPhases = sharedPhases
         this.phases = new Map()
         this.project = project

         phasesDefinition.phases.forEach( (phaseDefinition, i) => {
            this.addPhase(phaseDefinition, ec.newSubPath("phases",i))
         })
      }

      /** @param {ErrorContext} ec @return {void} */
      validateSync(ec) {
         DependencyGraph.validateSubset(this.sharedPhases.nodes,this.phases,ec)
         for (const phase of this.phases.values())
            phase.validatePrePost(ec)
      }

      /** @param {ErrorContext} ec */
      async validate(ec) {
         for (const phase of this.phases.values())
            await phase.validate(ec)
      }

      ///////////

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.phases.get(name) ?? error(`Phase name=${JSON.stringify(name)} does not exist.`,new ErrorContext({fct:"phase(name)",project:this.project.name})) }

      ///////////

      /** @param {JSPO_Phase_Definition} phaseDefinition @param {ErrorContext} [ec] */
      addPhase(phaseDefinition, ec) {
         ec ??= new ErrorContext({fct:"addPhase(p)",accesspath:["p"],project:this.project.name})
         typeof phaseDefinition === "object" || error(`Expect ${ec.ap()} of type object instead of »${typeof phaseDefinition}«.`,ec)
         const phaseName = phaseDefinition.phase
         typeof phaseName === "string" || error(`Expect ${ec.ap("phase")}=${JSON.stringify(phaseName)} of type string instead of »${typeof phaseName}«.`,ec)
         this.phases.has(phaseName) && error(`Expect ${ec.ap("phase")}=${JSON.stringify(phaseName)} to be unique.`,ec)
         ec = ec.newPhase(phaseName)
         if (phaseDefinition.prePhase !== undefined) {
            typeof phaseDefinition.prePhase === "string" || error(`Expect ${ec.ap("prePhase")} of type string instead of »${typeof phaseDefinition.prePhase}«.`,ec)
            phaseDefinition.prePhase !== phaseName || error(`Expect ${ec.ap("prePhase")} = »${String(phaseName)}« to reference not itself.`,ec)
         }
         if (phaseDefinition.postPhase !== undefined) {
            typeof phaseDefinition.postPhase === "string" || error(`Expect ${ec.ap("postPhase")} of type string instead of »${typeof phaseDefinition.postPhase}«.`,ec)
            phaseDefinition.postPhase !== phaseName || error(`Expect ${ec.ap("postPhase")} = »${String(phaseName)}« to reference not itself.`,ec)
         }
         if (phaseDefinition.dependsOnPhases) {
            Array.isArray(phaseDefinition.dependsOnPhases) || error(`Expect ${ec.ap("dependsOnPhases")} of type Array instead of »${typeof phaseDefinition.dependsOnPhases}«.`,ec)
            phaseDefinition.dependsOnPhases.forEach( (name, i) => typeof name === "string" || error(`Expect ${ec.ap("dependsOnPhases",i)} of type string instead of ${typeof name}.`,ec))
         }
         if (phaseDefinition.dependsOnProjects) {
            Array.isArray(phaseDefinition.dependsOnProjects) || error(`Expect ${ec.ap("dependsOnProjects")} of type Array instead of »${typeof phaseDefinition.dependsOnProjects}«.`,ec)
            phaseDefinition.dependsOnProjects.forEach( (o, i) => (typeof o === "object" || error(`Expect ${ec.ap("dependsOnProjects",i)} of type object instead of ${typeof o}.`,ec))
               && (typeof o.project === "string" || error(`Expect attribute ${ec.ap("dependsOnProjects",i,"project")} of type string instead of ${typeof o.project}.`,ec))
               && (typeof o.phase === "string" || error(`Expect ${ec.ap("dependsOnProjects",i,"phase")} of type string instead of ${typeof o.phase}.`,ec))
            )
         }
         if (phaseDefinition.builds) {
            Array.isArray(phaseDefinition.builds) || error(`Expect ${ec.ap("builds")} of type Array instead of »${typeof phaseDefinition.builds}«.`,ec)
         }
         const newPhase = this.sharedPhases.ensureNode(this.project.name,phaseName).setOwner(this.project)
         const createdFrom = newPhase
         /** @param {string} name @param {string} [projectName] @return {BuildPhase} */
         const getPhase = (name, projectName) => this.sharedPhases.ensureNode(projectName ?? this.project.name, name, createdFrom)
         const dependsOnPhases = (phaseDefinition.dependsOnPhases ?? []).map( (name) => getPhase(name))
         const dependsOnProjects = (phaseDefinition.dependsOnProjects ?? []).map( ({project,phase}) => getPhase(phase,project))
         const prePhase = phaseDefinition.prePhase === undefined ? undefined : getPhase(phaseDefinition.prePhase)
         const postPhase = phaseDefinition.postPhase === undefined ? undefined : getPhase(phaseDefinition.postPhase)
         phaseDefinition.builds?.forEach( (build, i) => newPhase.addBuild(build,ec.newSubPath("builds",i)))
         newPhase.setPrePost(prePhase,postPhase).setDependency(dependsOnProjects.concat(dependsOnPhases),ec)
         this.phases.set(phaseName, newPhase)
         this.validateSync(ec)
      }
   }

   class BuildProject {
      /** @type {string} */
      name
      /** @type {ProjectPhases} */
      projectPhases

      /** @param {string} name @param {JSPO_Project_Definition} phasesDefinition @param {SharedPhases} sharedPhases @param {ErrorContext} ec */
      constructor(name, phasesDefinition, sharedPhases, ec) {
         this.name = name
         this.projectPhases = new ProjectPhases(this, phasesDefinition, sharedPhases, ec.newProject(this.name))
      }

      /** @param {string} name @return {BuildPhase} */
      phase(name) { return this.projectPhases.phase(name) }

      /** @param {JSPO_Phase_Definition} phaseDefinition */
      addPhase(phaseDefinition) { this.projectPhases.addPhase(phaseDefinition) }

      /** @param {ErrorContext} [ec] @return {Promise<void>} */
      async validate(ec) { return this.projectPhases.validate(ec?.newProject(this.name) ?? new ErrorContext({fct:"validate",project:this.name})) }

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

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) {
         ec ??= new ErrorContext({fct:"validate()"})
         for (const project of this.projects.values())
            await project.validate(ec)
         await this.sharedPhases.validate(ec)
      }

      ///////////

      /** @param {string} name @return {BuildProject} */
      project(name) { return this.projects.get(name) || error(`Project »${String(name)}« is not defined.`,new ErrorContext({fct:"project(name)"})) }

      /** @param {string} name @return {boolean} */
      has(name) { return this.projects.has(name) }

      ///////////

      /** @param {JSPO_Project_Definition} projectDefinition */
      addProject(projectDefinition) {
         const getEC = () => new ErrorContext({fct:"addProject(p)",accesspath:["p"]})
         typeof projectDefinition === "object" || error(`Expect p of type string instead of »${typeof projectDefinition}«.`,getEC())
         const projectName = projectDefinition?.project
         typeof projectName === "string" || error(`Expect p.project = »${projectName}« of type string instead of »${typeof projectName}«.`,getEC())
         this.projects.has(projectName) && error(`Expect p.project = »${projectName}« to be unique.`,getEC())
         const newProject = new BuildProject(projectName, projectDefinition, this.sharedPhases, getEC())
         this.projects.set(projectName, newProject)
      }
   }

   class SharedPhases extends DependencyGraph {
      /** @type {Map<string,BuildPhase>}  */
      nodes
      /** @type {BuildProjects} owner of this object */
      projects

      /** @param {BuildProjects} projects */
      constructor(projects) {
         super()
         this.nodes = new Map()
         this.projects = projects
      }

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) {
         // !! should be catched by project.validate !!
         DependencyGraph.validate(this.nodes,ec)
      }

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) { return ProcessingStep.create(node) }

      /** @param {number} level @param {ErrorContext} ec @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level, ec) { return DependencyGraph.getProcessingSteps(this.nodes, level, SharedPhases.newProcessingStep, ec) }

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

/*
jspo.addProject({
   project: "cycle",
   phases: [ { phase: "build" } ]
})
*/

jspo.addProject({
   project: "default",
   phases: [
      { phase: "pre-clean" },
      { phase: "post-clean" },
      // { phase: "dep1", dependsOnProjects:[{project:"cycle",phase:"dep1"}] },
      { phase: "xxx", prePhase: "pre-clean", postPhase: "post-clean", //dependsOnPhases:["xxx"],
        builds: [
                  // files xxx2 and xxx3 must pre exist (no build rule) else build error (use touch xxx2 / xxx3)
                  { file:"xxx1", dependsOnFiles:["xxx2","xxx3"], action: async ()=>console.log("build xxx1") },
                  { file:"xxx10", dependsOnFiles:["xxx1","xxx3"], action: async ()=>console.log("build xxx10") },
                  { command:"xxx", action:async ()=>console.log("xxx commands") },
                ]
      },
      { phase: "clean", prePhase: "pre-clean", postPhase: "post-clean" },
      { phase: "build-javascript-modules", dependsOnPhases: ["clean","xxx"], dependsOnProjects: [{project:"build-javascript",phase:"build"}] },
   ]
})

// jspo.addProject({project:"cycle",phases:[{phase:"dep1", dependsOnProjects:[{project:"default",phase:"dep1"}]}]})
// await jspo.project("default").validate()

jspo.switchProject("default", "build-javascript-modules")

// project: "default" && phase: "build-javascript-modules"
jspo.addBuild({ file:"test.mjs", dependsOnFiles:["html-snippets/test.js"], action: async ({ec}) => {
   console.log("Build test.mjs")
   await jspo.files.overwrite("test.mjs",
      jspo.appendESModuleExport(
            await jspo.files.loadString("html-snippets/test.js",ec),
            [ "TEST", "RUN_TEST", "SUB_TEST", "END_TEST", "RESET_TEST", "NEW_TEST" ]
      ),
   )
}})
jspo.addBuild({ command:"test.mjs postprocessor", dependsOnFiles:["test.mjs"], action:async ()=>console.log("test.mjs postprocessor") })

const cleanPhase = jspo.phase("clean")
cleanPhase.addBuild({ command:"remove-files", dependsOnCommands:[], action:async ()=>console.log("remove files") })
cleanPhase.addBuild({ command:"remove-dirs", dependsOnCommands:["remove-files"], action:async ()=>console.log("remove directories") })
jspo.phase("pre-clean").addBuild({ command:"pre-clean", action:async ()=>console.log("pre-clean commands") })
jspo.phase("post-clean").addBuild({ command:"post-clean", action:async ()=>console.log("post-clean commands") })

await jspo.validateProjects()

console.log("=== cleanPhase.build() ===")

await cleanPhase.build()

console.log("\n=== jspo.build() ===")

await jspo.buildSteps()
   // .then( steps => console.log(steps))
   .then( () => jspo.build())
   .catch( e => console.log("error",e))

// test validation cycle error (uncomment cycles for test)

jspo.addProject({
   project: "project-with-cycles-1",
   phases: [ { phase: "build", dependsOnProjects:[{project:"project-with-cycles-2",phase:"build"}]} ]
})

jspo.projects.addProject({
   project: "project-with-cycles-2",
   phases: [ { phase: "build", dependsOnProjects:[/*{project:"project-with-cycles-1",phase:"build"}*/],
               builds: [
                  { command:"c1", dependsOnCommands:["c2"], action: async ()=>console.log("build c1") },
                  { command:"c2", dependsOnCommands:["c3"], action: async ()=>console.log("build c2") },
                  { command:"c3", dependsOnCommands:["c4"], action: async ()=>console.log("build c3") },
                  { command:"c4", dependsOnCommands:[]/*"c2"]*/, action: async ()=>console.log("build c4") },
               ] } ]
})

console.log("\n=== jspo.validateProjects() ===")

await jspo.validateProjects().catch(e => {
   console.log("\n"+String(e))
})

// jspo.project("project-with-cycles-2").phase("build").build()
