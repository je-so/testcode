// JavaScript Projects Organizer
// (c) 2025 Jörg Seebohn

// First version allows to create an ES module from a JavaScript file
// *.js -> *.mjs

// !! execute as unprivileged user to prevent any accidental damage to the file system !!
// run with
// > bun run jspo.js ::build:c4 # matches all projects with default config with a "build" phase and a "c4" build rule
// or
// > bun run jspo.js default:release # matches all phases of default project with release config
// or
// > bun run jspo.js build # matches the "build" target (defined with addTarget) which defines one or more project:phase:build
// or
// > bun run -l # lists projects and targets
import * as fs from "node:fs"

const jspo = (() => {

   /**
    * @typedef JSPO_Common_ProjectConfig
    * @property {string} config Name of configuration.
    * @property {true} [default] Must be set to true for the default configuration. Which is used if no configuration is set or given at the command line.
    *
    * @typedef {JSPO_Common_ProjectConfig & JSPO_ProjectConfig} JSPO_ProjectConfig_Definition
    *
    * @typedef JSPO_BuildRef_Definition
    * @property {string} project
    * @property {string} [config] Missing config uses default configuration (see setDefaultConfig).
    * @property {string} phase
    * @property {string} [build]
    *
    * @typedef JSPO_Target_Definition
    * @property {string} target
    * @property {string} [comment]
    * @property {JSPO_BuildRef_Definition[]} builds
    *
    * @typedef JSPO_Project_Definition
    * @property {string} project
    * @property {string|JSPO_ProjectConfig_Definition} [config] Missing config uses default configuration (see setDefaultConfig).
    * @property {string} [comment]
    * @property {JSPO_Phase_Definition[]} phases
    *
    * @typedef JSPO_Phase_Definition
    * @property {string} phase
    * @property {string} [comment]
    * @property {(string|JSPO_PhaseRef_Definition)[]} [dependsOnPhases]
    * @property {string|JSPO_PhaseRef_Definition} [prePhase]
    * @property {string|JSPO_PhaseRef_Definition} [postPhase]
    * @property {JSPO_BuildRule_Definition[]} [builds]
    *
    * @typedef JSPO_PhaseRef_Definition
    * @property {string} [project] Missing project uses project this phase is part of.
    * @property {string} [config] Missing config uses default configuration (see setDefaultConfig).
    * @property {string} phase
    *
    * @typedef JSPO_BuildAction_InitArg
    * @property {string} project
    * @property {BuildConfig} config
    * @property {string} path
    * @property {string[]} dependsOnBuilds
    *
    * @typedef JSPO_BuildAction_CallArg
    * @property {string} project
    * @property {BuildConfig} config
    * @property {string} path
    * @property {string[]} dependsOnBuilds
    * @property {ErrorContext} ec
    *
    * @typedef {(arg:JSPO_BuildAction_CallArg)=>Promise<void>} JSPO_BuildAction
    *
    * @typedef JSPO_CommandBuildRule_Definition
    * @property {string} command
    * @property {string[]} [dependsOnBuilds]
    * @property {JSPO_BuildAction} action
    *
    * @typedef JSPO_FileBuildRule_Definition
    * @property {string} file
    * @property {string[]} [dependsOnBuilds]
    * @property {JSPO_BuildAction} action
    *
    * @typedef {JSPO_CommandBuildRule_Definition|JSPO_FileBuildRule_Definition} JSPO_BuildRule_Definition
    *
    * @typedef JSPO_ErrorContext_Arg
    * @property {string} fct
    * @property {(number|string)[]} [accesspath]
    * @property {undefined|string} [project]
    * @property {undefined|string} [config]
    * @property {undefined|string} [phase]
    * @property {undefined|string} [build]
    *
    */

   class ErrorContext {
      /** @param {JSPO_ErrorContext_Arg} arg */
      constructor(arg) {
         this.fct = arg.fct
         this.accesspath = arg.accesspath ?? []
         this.project = arg.project
         this.config = arg.config
         this.phase = arg.phase
         this.build = arg.build
      }

      /** @param {string} build */
      newBuild(build) { return new ErrorContext({...this,build}) }
      /** @param {string} config */
      newConfig(config) { return new ErrorContext({...this,config}) }
      /** @param {string} phase */
      newPhase(phase) { return new ErrorContext({...this,phase}) }
      /** @param {string} project @param {string} [phase] */
      newProject(project, phase) { return new ErrorContext(phase?{...this,project,phase}:{...this,project}) }
      /** @param {(number|string)[]} subpath */
      newAccessPath(...subpath) { return new ErrorContext({...this,accesspath:this.accesspath.concat(subpath)}) }

      /** @param {(number|string)[]} subpath @return {string} Returns access path for argument which caused an error. */
      ap(...subpath) {
         const accesspath = this.accesspath.concat(subpath)
         let path = accesspath.length ? String(accesspath[0]) : ""
         for (let i=1; i<accesspath.length; ++i)
            path += typeof accesspath[i] === "number" ? "["+accesspath[i]+"]" : "."+accesspath[i]
         return path
      }
      errcontext() {
         const forProject = (this.project === undefined ? "" : ` project ${JSON.stringify(this.project)}`)
                            + (this.config === undefined ? "" : ` config ${JSON.stringify(this.config)}`)
                            + (this.phase === undefined ? "" : ` phase ${JSON.stringify(this.phase)}`)
                            + (this.build === undefined ? "" : ` build ${JSON.stringify(this.build)}`)
         return `${this.fct} failed${forProject?" for":""}${forProject}`
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

   /** @param {any} value @param {string} exptectedType @param {ErrorContext} context */
   const expectTypeError = (value, exptectedType, context) => error(`Expect ${context.ap()} of type ${exptectedType} instead of ${value === null && "null" || typeof value === "object" && value?.constructor?.name || typeof value}.`,context)

   /** @param {string} name @param {()=>ErrorContext} getEC */
   const validateName = (name, getEC) => {
      typeof name === "string" || expectTypeError(name,"string",getEC())
      name === name.trim() || error(`Expect ${getEC().ap()}=${JSON.stringify(name)} to have no leading or trailing spaces.`,getEC())
      name === "" && error(`Expect ${getEC().ap()}=${JSON.stringify(name)} not empty.`,getEC())
      const match = name.match(/[":,]/)
      match && error(`Expect ${getEC().ap()}=${JSON.stringify(name)} to not contain character »${match[0]}«.`,getEC())
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
      /** @type {CommandLineProcessor} */
      #commandLine
      /** @type {Files} */
      #files
      /** @type {BuildTargets} */
      #targets
      /** @type {BuildProjects} */
      #projects

      constructor() {
         this.#commandLine = new CommandLineProcessor(Bun.argv.slice(2)) // argv: [ "path/to/bun", "path/to/jspo.js", "arg1", "arg2", ... ]
         this.#files = new Files()
         this.#targets = new BuildTargets()
         this.#projects = new BuildProjects()
      }

      /** @return {CommandLineProcessor} */
      get commandLineProcessor() { return this.#commandLine }
      /** @type {Files} Allows to manipulate system files. */
      get files() { return this.#files }
      /** @return {string[]} */
      get configNames() { return this.#projects.sharedConfigs.configNames }
      /** @return {BuildConfig} */
      get defaultConfig() { return this.#projects.defaultConfig }
      /** @type {string[]} */
      get targetNames() { return this.#targets.targetNames }
      /** @param {undefined|string} config @return {BuildConfig} */
      config(config) { return this.#projects.sharedConfigs.config(config,()=>new ErrorContext({fct:"config(p)",accesspath:["p"]})) }
      /** @param {string} project @param {undefined|string} config @param {string} phase @return {BuildPhase} */
      phase(project, config, phase) { return this.project(project,config).phase(phase) }
      /** @param {string} project @param {string} [config] @return {BuildPhase[]} */
      phases(project, config) { return [...this.project(project,config).phases.phases.values()] }
      /** @param {string} project @param {string} [config] @return {BuildProject} */
      project(project, config) { return this.#projects.project(project,config) }
      /** @param {undefined|string} config @return {BuildProject[]} */
      projects(config) { return this.#projects.projectsByConfig(config,()=>new ErrorContext({fct:"projects(config)",accesspath:["config"]})) }
      /** @param {string} [config] @return {string[]} */
      projectNames(config) { return this.#projects.projectsByConfig(config,()=>new ErrorContext({fct:"projectNames(config)",accesspath:["config"]})).map(p => p.name) }
      /** @param {string} name @return {JSPO_Target_Definition} */
      target(name) { return this.#targets.target(name) }

      /** @param {(config:BuildConfig)=>void} callback */
      forEachConfig(callback) {
         for (const name of this.#projects.sharedConfigs.configNames)
            callback(this.config(name))
      }

      /** @param {JSPO_BuildRef_Definition} ref @return {JSPO_BuildRef_Definition[]} */
      matchBuildRef(ref) {
         const matchedTargets = []
         const config = jspo.config(ref.config || undefined).name, build = ref.build || undefined
         for (const project of jspo.projectNames(config)) {
            if (project !== ref.project && ref.project) continue
            for (const phase of jspo.project(project,config).phaseNames) {
               if (phase !== ref.phase && ref.phase) continue
               if (build && !jspo.phase(project,config,phase).hasBuild(build)) continue
               matchedTargets.push({project,config,phase,build})
            }
         }
         return matchedTargets
      }

      ///////////

      /** @param {string} project @param {undefined|string} config @param {string} phase @param {JSPO_BuildRule_Definition} build */
      addBuild(project, config, phase, build) { this.phase(project,config,phase).addBuild(build) }

      /** @param {string} project @param {undefined|string} config @param {JSPO_Phase_Definition} phase */
      addPhase(project, config, phase) { this.project(project,config).addPhase(phase) }

      /** @param {JSPO_Project_Definition} project */
      addProject(project) { this.#projects.addProject(project) }

      /** @param {...JSPO_Target_Definition} targets */
      addTargets(...targets) { this.#targets.addTargets(targets) }

      /** @param {...JSPO_ProjectConfig_Definition} configs */
      addConfigs(...configs) { this.#projects.sharedConfigs.addConfigs(...configs) }

      /** @param {JSPO_ProjectConfig_Definition} config */
      setDefaultConfig(config) { this.#projects.sharedConfigs.setDefaultConfig(config) }

      ///////////

      /** @return {Promise<void>} */
      async validateProjects() { return this.#projects.validate(new ErrorContext({fct:"validateProjects()"})) }

      /** @param {string} project @param {undefined|string} config @param {string} phase @param {string[]} [builds] @return {Promise<void>} */
      async build(project, config, phase, builds) { return this.phase(project,config,phase).build(builds) }

      /** @param {string} project @param {undefined|string} config @param {string} phase @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps(project, config, phase) { return this.phase(project,config,phase).buildSteps() }

      ////////////////////////////
      // Command-Line interface //
      ////////////////////////////

      async processCommandLine() { return this.#commandLine.process() }

      ///////////

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

   class CommandLineProcessor {

      /** @param {string[]} args command-line arguments */
      constructor(args) {
         this.args = args
      }

      async process() {
         console.log("\nJavaScript Projects Organizer v0.2.0\n")
         let i = 0
         for (const arg of this.args) {
            if (arg === "-l" || arg === "--list") this.listProjects(), this.listTargets()
            else if (arg === "-lp" || arg === "--list-project") this.listProjects()
            else if (arg === "-lt" || arg === "--list-target") this.listTargets()
            else {
               const matchedTargets = []
               console.log("-- Match target --")
               console.log(arg,"\n")
               if (arg.includes(":")) {
                  const split = arg.split(":")
                  matchedTargets.push(...jspo.matchBuildRef({project:split[0],config:split[1],phase:split[2],build:split[3]}))
               }
               else {
                  for (const target of jspo.targetNames)
                     if (target === arg)
                        matchedTargets.push(...jspo.target(target).builds)
               }
               if (matchedTargets.length === 0)
                  console.log("Found no builds\n")
               else {
                  console.log("Found build(s)")
                  console.group()
                  for (const { project, config, phase, build } of matchedTargets)
                     console.log(`project:${JSON.stringify(project)}${config?" config:"+JSON.stringify(config):""} phase:${JSON.stringify(phase)}${build?" build:"+JSON.stringify(build):""}`)
                  console.groupEnd()
                  console.log()
                  for (const {project,config,phase,build} of matchedTargets) {
                     await jspo.phase(project,config,phase).build(build ? [build] : undefined)
                  }
               }
            }
            ++i
         }
      }

      listProjects() {
         const configNames = jspo.configNames
         const projectNames = []
         for (const config of configNames)
            projectNames.push(...jspo.projectNames(config))
         console.log("-- List of projects --")
         if (projectNames.length === 0)
            console.log("no projects defined")
         else {
            for (const config of configNames)
               for (const projectName of jspo.projectNames(config)) {
                  const project = jspo.project(projectName,config)
                  console.log("Project:")
                  console.log(" name:",JSON.stringify(projectName),"config:",JSON.stringify(config))
                  if (project.comment) console.log(" comment:",project.comment)
                  console.log(" phases:",JSON.stringify(project.phaseNames))
               }
         }
         console.log()
      }

      listTargets() {
         const targetNames = jspo.targetNames
         console.log("-- List of targets --")
         if (targetNames.length === 0)
            console.log("no targets defined")
         else {
            for (const targetName of targetNames) {
               const target = jspo.target(targetName)
               console.log("Target:")
               console.log(" name:",JSON.stringify(targetName))
               if (target.comment) console.log(" comment:",target.comment)
               console.log(" builds:",JSON.stringify(target?.builds))
            }
         }
         console.log()
      }

      printUsage() {
         console.log("\nusage: bun run jspo.js [option]... target")
         console.log("options:")
         console.group("> '-l' or '--list'")
         console.log("Lists all defined projects and their phases.\n")
         console.groupEnd()
      }
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
         typeof path === "string" || expectTypeError(path,"string",new ErrorContext({fct:"new DependentNode(path)",accesspath:["path"]}))
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
         Array.isArray(dependentOn) || expectTypeError(dependentOn,"Array",ec)
         /** @param {any} node @param {number} i */
         const checkType = (node,i) => { node instanceof nodeType || expectTypeError(node,nodeType.name,ec.newAccessPath(i)) }
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

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) { error(`validate() not overwritten in subtype (internal error).`,ec) }

      /** @param {number} level @param {ErrorContext} ec @param {null|Map<string,DependentNode>} [subset] @return {Promise<ProcessingStep[]>} */
      async getProcessingSteps(level, ec, subset) { return error(`getProcessSteps() not overwritten in subtype (internal error).`,ec) }

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
       * @param {T[]} nodes
       * @return {Map<string,T>}
       */
      static getClosure(nodes) {
         /** @type {Set<DependentNode>} */
         const nodeSet = new Set()
         /** @type {DependentNode[]} */
         const unprocessed = [...nodes]
         for (let nextNode; nextNode = unprocessed.shift(); ) {
            if (nodeSet.has(nextNode)) continue
            nodeSet.add(nextNode)
            unprocessed.push(...nextNode.dependentOn)
         }
         /** @type {Map<string,any>} */
         const closureMap = new Map()
         nodeSet.forEach(node => closureMap.set(node.path,node))
         return closureMap
      }

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes
       * @param {undefined|null|string[]} paths Optional subset of nodes encoded as set of paths.
       * @param {string} nodeTypeName
       * @param {(path:string)=>ErrorContext} getEC Allows for calling newBuild, newPhase, or newProject
       * @return {Map<string,T>}
       */
      static getSubset(nodes, paths, nodeTypeName, getEC) {
         if (! paths?.length) return nodes
         const subset = new Set()
         paths.forEach( (path, i) => {
            const node = nodes.get(path)
            node || error(`Found no ${nodeTypeName} for ${getEC(path).ap(i)}=${JSON.stringify(path)}.`,getEC(path))
            subset.add(node)
         })
         return this.getClosure([...subset])
      }


      ////////////////
      // Validation //
      ////////////////

      /** @param {Map<string,DependentNode>} nodes @param {string} nodeTypeName @param {ErrorContext} ec @return {void} */
      static validate(nodes, nodeTypeName, ec) {
         this.validateAllNodesContained(nodes, nodeTypeName, ec)
         this.validateNoCycles(nodes, nodeTypeName, ec)
      }

      /** @param {Map<string,DependentNode>} nodes @param {string} nodeTypeName @param {ErrorContext} ec */
      static validateAllNodesContained(nodes, nodeTypeName, ec) {
         for (const node of nodes.values())
            node.dependentOn.forEach((node2, i) => nodes.get(node2.path) === node2 || error(`${nodeTypeName}=${JSON.stringify(node.path)}.dependentOn[${i}] references uncontained node ${JSON.stringify(node2.path)} (internal error).`,ec))
      }

      /** @param {Map<string,DependentNode>} nodes @param {string} nodeTypeName @param {ErrorContext} ec */
      static validateNoCycles(nodes, nodeTypeName, ec) { this.getProcessingSteps(nodes, nodeTypeName, 0, ProcessingStep.create, ec) }

      ///////////////////////////////////
      // Layout Nodes in Process Order //
      ///////////////////////////////////

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes
       * @param {string} nodeTypeName "BuildNode", or "command or file node", ...
       * @param {number} level
       * @param {(node:DependentNode)=>TProcessingStep<T>} newProcessingStep
       * @param {ErrorContext} ec
       * @return {TProcessingStep<T>[]}
       */
      static getProcessingSteps(nodes, nodeTypeName, level, newProcessingStep, ec) {
         return this.orderSteps(this.wrapNodes(nodes, newProcessingStep),nodeTypeName,level,ec)
      }

      /**
       * @template {DependentNode} T
       * @param {Map<string,T>} nodes Must be a closure (nodes must contain all node.dependentOn for every node in nodes)
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

      /** @param {TProcessingStep<DependentNode>[]} steps @param {string} nodeTypeName @param {ErrorContext} ec */
      static cycleError(steps, nodeTypeName, ec) {
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
               cycle && error(`Dependency cycle detected consisting of ${nodeTypeName}${cycle.length===1?"":"(s)"} ${cycle.map(s=>`"${s.node.canonicalName}"`).join(", ")}.`,ec)
            }
         }
         error(`Dependency cycle detected (internal error).`,ec)
      }

      /**
       * @template {DependentNode} T
       * @param {TProcessingStep<T>[]} steps
       * @param {string} nodeTypeName
       * @param {number} level
       * @param {ErrorContext} ec
       * @return {TProcessingStep<T>[]}
       */
      static orderSteps(steps, nodeTypeName, level, ec) {
         if (typeof level !== "number") level = 0
         const orderedSteps = []
         let nextSteps = steps.filter( step => step.unprocessedDependentOn === 0)
         while (orderedSteps.length < steps.length) {
            if (nextSteps.length === 0)
               this.cycleError(steps, nodeTypeName, ec)
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
      /** @type {JSPO_BuildAction_InitArg} */
      arg
      /**
       * @param {JSPO_BuildAction} action
       * @param {JSPO_BuildAction_InitArg} arg
       */
      constructor(action, arg) {
         this.action = action
         this.arg = arg
      }
      /** @param {ErrorContext} ec */
      async call(ec) { return this.action({...this.arg,ec}) }
   }

   class BuildNodeStrategy {
      /** @return {boolean}  */
      get isVirtual() { return error("get isVirtual not implemented in subtype (internal error).",new ErrorContext({fct:"isVirtual()"})) }
      /** @return {string}  */
      get type() { return error("get type not implemented in subtype (internal error).",new ErrorContext({fct:"type()"})) }
      /** @param {BuildNode} build @return {Promise<undefined|number>}  */
      async mtime(build) { return error("mtime not implemented in subtype (internal error).",new ErrorContext({fct:"mtime()"})) }
      /** @param {BuildNode} build @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(build,updateNodes) { return error("needsUpdate not implemented in subtype (internal error).",new ErrorContext({fct:"needsUpdate()"})) }
   }

   /**
    * Describes a single potential build step.
    * A build step wraps a BuildNode and is of type TProcessingStep<BuildNode>.
    */
   class BuildNode extends DependentNode {
      /** @type {"BuildNode"} */
      static TYPENAME="BuildNode"
      /** @type {BuildNodeStrategy} Implementation which differs between different types of nodes. */
      strategy
      /** @type {undefined|BuildRule} */
      buildRule
      /** @type {string} Path of node which referenced this node the first time. */
      firstReferencedFrom
      /** @param {string} path @param {string} [firstReferencedFrom] */
      constructor(path, firstReferencedFrom) {
         super(path)
         this.strategy = new BuildNodeFileStrategy()
         this.firstReferencedFrom = firstReferencedFrom ?? this.canonicalName
      }
      get canonicalName() { return `${this.type}:${this.path}` }
      /** @return {boolean}  */
      get isDefined() { return this.buildRule !== undefined }
      /** @return {boolean}  */
      get isVirtual() { return this.strategy.isVirtual }
      /** @return {string}  */
      get type() { return this.strategy.type }
      /** @return {Promise<undefined|number>}  */
      async mtime() { return this.strategy.mtime(this) }
      /** @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(updateNodes) { return this.strategy.needsUpdate(this,updateNodes) }

      /** @param {BuildRule} buildRule @param {BuildNodeStrategy} strategy */
      setBuildStrategy(buildRule, strategy) { this.buildRule = buildRule; this.strategy = strategy; }
      /** @param {BuildNode[]} dependentOn @param {ErrorContext} ec */
      setDependency(dependentOn, ec) { super.setDependency(dependentOn, ec, BuildNode) }
   }

   class BuildNodeFileStrategy {
      get isVirtual() { return false }
      get type() { return "file" }
      /** @param {BuildNode} build */
      async mtime(build) { return jspo.files.mtime(build.path) }
      /** @param {BuildNode} build @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(build,updateNodes) {
         const existNode = await jspo.files.exists(build.path)
         if (!existNode) return true
         const mtime = await this.mtime(build)
         for (const source of build.typedDependentOn)
            if (updateNodes.has(source) || mtime < Number(await source.mtime()))
               return true
         return false
      }
   }

   class BuildNodeCommandStrategy {
      get isVirtual() { return true }
      get type() { return "command" }
      /** @param {BuildNode} build */
      async mtime(build) { return undefined }
      /** @param {BuildNode} build @param {Set<BuildNode>} updateNodes @return {Promise<boolean>} */
      async needsUpdate(build,updateNodes) {
         const dependentOn = build.typedDependentOn
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
      /** @type {Map<string,BuildNode>} Stores all CommandNodes/FileNodes indexed by file-path or command-name which could interdepend on each other. */
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

      /** @param {ErrorContext} ec @return {void} */
      validateSync(ec) {
         DependencyGraph.validate(this.buildNodes,BuildNode.TYPENAME,ec)
      }

      /** @param {ErrorContext} ec */
      async validate(ec) {
         if (this.validated) return
         ec = ec?.newPhase(this.phase.name) ?? new ErrorContext({fct:"validate()",project:this.phase.projectName,phase:this.phase.name})
         for (const node of this.buildNodes.values())
            if (!node.buildRule && (node.isVirtual || !(await jspo.files.exists(node.path))))
               error(`Undefined build rule for ${JSON.stringify(node.canonicalName)} referenced from ${JSON.stringify(node.firstReferencedFrom)}.`,ec.newBuild(node.canonicalName))
         this.validated = true
      }

      /** @param {number} level @param {ErrorContext} ec @param {null|Map<string,BuildNode>} [subset] @return {Promise<TProcessingStep<BuildNode>[]>} */
      async getProcessingSteps(level, ec, subset) {
         const updateNodes = new Set()
         /** @type {(node:BuildNode)=>TProcessingStep<BuildNode>} */
         const newProcessingStep = ProcessingStep.create
         const buildNodes = subset ?? this.buildNodes
         const steps = DependencyGraph.getProcessingSteps(buildNodes,BuildNode.TYPENAME,level,newProcessingStep,ec)
         for (const step of steps) {
            if (step.needUpdate) {
               step.needUpdate = await step.node.needsUpdate(updateNodes)
               if (step.needUpdate) updateNodes.add(step.node)
            }
         }
         return steps
      }

      ///////////

      /** @return {BuildConfig} */
      get config() { return this.phase.config }
      /** @return {string} */
      get project() { return this.phase.projectName }

      /** @param {string} path @return {boolean} */
      hasBuild(path) { return this.buildNodes.has(path) }

      ///////////

      /** @param {JSPO_BuildRule_Definition} build @param {ErrorContext} [ec] */
      addBuild(build, ec) {
         const getEC = () => ec ?? new ErrorContext({fct:"addBuild(p)",accesspath:["p"],project:this.phase.projectName,phase:this.phase.name})
         typeof build === "object" || expectTypeError(build,"object",getEC())
         this.validated = false
         const isCommand = "command" in build
         const isFile = "file" in build
         isCommand || isFile || error(`Expect ${getEC().ap("command")} or ${getEC().ap("file")} to be defined.`,getEC())
         const path = isCommand ? build.command : build.file
         validateName(path, () => getEC().newAccessPath(isCommand?"command":"file"))
         this.buildNodes.get(path)?.isDefined && error(`Expect ${getEC().ap(isCommand?"command":"file")} = »${path}« to be unique.`,getEC())
         const dependsOnBuilds = build.dependsOnBuilds ?? []
         const action = build.action
         typeof action === "function" || error(`Expect ${getEC().ap("action")} of type function instead of »${typeof action}«.`,getEC())
         /** @param {BuildNode} node */
         const firstDefinedErrorMsg = (node) => node.isDefined ? `but node ${JSON.stringify(node.path)} is defined as ${node.type} node.` : `but node ${JSON.stringify(node.path)} is referenced as a ${node.type} node from ${JSON.stringify(node.firstReferencedFrom)}.`
         /** @param {string[]} dependsOn @param {string} argName @param {boolean} isCommand */
         const checkType = (dependsOn, argName, isCommand) => {
            Array.isArray(dependsOn) || expectTypeError(dependsOn,"Array",getEC().newAccessPath(argName))
            dependsOn.forEach( (path,i) => typeof path === "string" || expectTypeError(path,"string",getEC().newAccessPath(argName,i)))
         }
         checkType(dependsOnBuilds,"dependsOnBuilds",true)
         /** @param {string} path @param {string} [firstReferencedFrom] */
         const getNode = (path, firstReferencedFrom) => DependencyGraph.ensureNode(path, this.buildNodes, () => new BuildNode(path,firstReferencedFrom))
         const newNode = getNode(path)
         const dependsOnNodes = dependsOnBuilds.map(path => getNode(path,newNode.canonicalName))
         newNode.setBuildStrategy(new BuildRule(action, { project:this.project, config:this.config, path, dependsOnBuilds }), isCommand ? new BuildNodeCommandStrategy() : new BuildNodeFileStrategy() )
         newNode.setDependency(dependsOnNodes,getEC().newAccessPath("dependsOnBuilds"))
         this.validateSync(getEC().newBuild(newNode.canonicalName))
      }

      ///////////

      /** @param {string[]} paths @param {ErrorContext} ec */
      getSubset(paths, ec) { return DependencyGraph.getSubset(this.buildNodes,paths,BuildNode.TYPENAME,(path)=>ec.newBuild(path)) }

      /** @param {ErrorContext} ec @param {null|Map<string,BuildNode>} [subset] @return {Promise<void>} */
      async build(ec, subset) {
         const steps = await this.getProcessingSteps(0,ec,subset)
         for (const step of steps) {
            const node = step.node
            if (step.needUpdate && node.buildRule) {
               this.logStep(step)
               console.group()
               await node.buildRule.call(ec.newBuild(node.canonicalName))
               if (!node.isVirtual)
                  await jspo.files.touch(node.path,ec.newBuild(node.canonicalName))
               console.groupEnd()
            }
         }
      }

      /** @param {ErrorContext} ec @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps(ec) { return this.getProcessingSteps(0,ec,null) }

      /** @param {TProcessingStep<BuildNode>} step */
      logStep(step) { console.log(`build ${step.node.canonicalName}`) }
   }

   /**
    * A build phase is a set of build steps (implemented as BuildNodes). It could
    * depend on other phases. Either of the same project or of another project.
    * A phase is therefore used to make a set of build steps depend on another project.
    * A phase supports also a pre-phase and post-phase which are additionaly processed.
    * If a pre-/port-phases are referenced from different phases they are are executed more
    * than once.
    */
   class BuildPhase extends DependentNode {
      /** @type {"BuildPhase"} */
      static TYPENAME="BuildPhase"
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
      /** @type {BuildConfig} */
      config
      /** @type {undefined|BuildProject} owner of this object */
      project
      /** @type {boolean} */
      validated
      /** @type {string} */
      comment

      /////////////////////////////
      // Overwrite DependentNode //
      /////////////////////////////

      /** @param {string} projectName @param {BuildConfig} config @param {string} phaseName @param {BuildPhase} [createdFrom] */
      constructor(projectName, config, phaseName, createdFrom) {
         super(BuildPhase.path(projectName,config,phaseName))
         this.buildNodes = new BuildNodes(this)
         this.createdFrom = createdFrom ?? this
         this.name = phaseName
         this.projectName = projectName
         this.config = config
         this.validated = false
         this.comment = ""
      }

      /** @param {BuildPhase[]} dependentOn @param {ErrorContext} ec */
      setDependency(dependentOn, ec) { super.setDependency(dependentOn, ec, BuildPhase) }

      /**
       * @typedef BuildPhase_Init_Arg
       * @property {BuildProject} project
       * @property {null|string} [comment]
       * @property {undefined|BuildPhase} prePhase
       * @property {undefined|BuildPhase} postPhase
       * @property {BuildPhase[]} dependentOn
       * @property {ErrorContext} ec
       * @param {BuildPhase_Init_Arg} arg @return {this}
       */
      init({project,comment,prePhase,postPhase,dependentOn,ec}) {
         if (this.projectName !== project.name) error(`Phase »${this.name}« is owned by ${JSON.stringify(this.projectName)} not ${JSON.stringify(project.name)} (internal error).`,ec)
         this.project = project
         this.comment = comment != null ? String(comment).trim() : ""
         this.prePhase = prePhase
         this.postPhase = postPhase
         this.setDependency(dependentOn,ec.newAccessPath("dependsOn"))
         return this
      }

      ///////////

      /** @param {string} project @param {BuildConfig} config @param {string} phase */
      static path(project, config, phase) {
         typeof project === "string" || expectTypeError(project,"string",new ErrorContext({fct:"path(project, config, phase)",accesspath:["project"]}))
         config instanceof BuildConfig || expectTypeError(config,"BuildConfig",new ErrorContext({fct:"path(project, config, phase)",accesspath:["config"]}))
         typeof phase === "string" || expectTypeError(phase,"string",new ErrorContext({fct:"path(project, config, phase)",accesspath:["phase"]}))
         return JSON.stringify(project)+","+JSON.stringify(config.name)+","+JSON.stringify(phase)
      }

      get canonicalName() { return `project:${this.projectName} config:${this.config.name} phase:${this.name}`}

      /** @type {boolean} */
      get isDefined() { return this.project !== undefined }

      /** @type {boolean} */
      get hasDependency() { return this.prePhase !== undefined || this.postPhase !== undefined || this.dependentOn.length>0 }

      /** @param {string} path @return {boolean} */
      hasBuild(path) { return this.buildNodes.hasBuild(path) }

      /** @param {ErrorContext} [ec] @return {Promise<void>} */
      async validate(ec) {
         if (this.validated && this.buildNodes.validated) return
         this.validated = true
         try {
            ec = ec?.newPhase(this.name) ?? new ErrorContext({fct:"validate()",project:this.projectName,phase:this.name})
            this.validatePrePost(ec)
            this.validateDefined(ec)
            await this.buildNodes.validate(ec)
            for (const phase of this.typedDependentOn)
               await phase.validate(ec)
         }
         catch(e) {
            this.validated = false
            throw e
         }
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
         const checkDefined = (phase) => !phase || phase.isDefined || error(`Undefined phase:${JSON.stringify(phase.canonicalName)} referenced from phase ${JSON.stringify(phase.createdFrom.canonicalName)}.`,ec)
         checkDefined(this.prePhase)
         checkDefined(this.postPhase)
         checkDefined(this)
      }

      ///////////

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
         const phases = DependencyGraph.getClosure([this])
         /** @type {TProcessingStep<BuildPhase>[]} */
         const steps = DependencyGraph.getProcessingSteps(phases, BuildPhase.TYPENAME, level, BuildPhase.newProcessingStep, ec.newPhase(this.name))
         return steps
      }

      /** @return {Promise<TProcessingStep<BuildNode>[]>} */
      async buildSteps() {
         const ec = new ErrorContext({fct:"buildSteps()",project:this.projectName,phase:this.name})
         const buildSteps = []
         const level = () => (buildSteps.length ? buildSteps[buildSteps.length-1].level+1 : 0)
         for (const step of await this.getProcessingSteps(level(),ec)) {
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            prePhase && buildSteps.push(...await prePhase.buildNodes.getProcessingSteps(level(),ec))
            buildSteps.push(...await phase.buildNodes.getProcessingSteps(level(),ec))
            postPhase && buildSteps.push(...await postPhase.buildNodes.getProcessingSteps(level(),ec))
         }
         return buildSteps
      }

      /** @param {null|string[]} [paths] Optional subset of BuildNodes encoded as set of paths. @return {Promise<void>} */
      async build(paths) {
         const ec = new ErrorContext({fct:"build()",project:this.projectName,phase:this.name})
         const subset = paths?.length ? this.buildNodes.getSubset(paths,new ErrorContext({fct:"build(paths)",accesspath:["paths"],project:this.projectName,phase:this.name})) : undefined
         this.logPhase(this,paths,true)
         for (const step of await this.getProcessingSteps(0,ec)) {
            if (!step.needUpdate) continue
            const prePhase = step.node.prePhase, phase = step.node, postPhase = step.node.postPhase
            this.logPhase(phase,phase===this?paths:undefined,false)
            console.group()
            prePhase && await prePhase.buildNodes.build(ec)
            await phase.buildNodes.build(ec,phase===this?subset:undefined)
            postPhase && await postPhase.buildNodes.build(ec)
            console.groupEnd()
         }
         console.log()
      }

      /** @param {BuildPhase} node @param {null|undefined|string[]} paths */
      logPhase(node, paths, toplevel) { console.log(`Build project:${JSON.stringify(node.projectName)} config:${JSON.stringify(node.config.name)} phase:${JSON.stringify(node.name)}${paths?.length?" build:"+JSON.stringify(paths.join(",")):""}${toplevel?"\n=====":""}`) }
   }

   /**
    * A set of build phases belonging to a project and which could depend on other phases of the same or another project.
    * Cyclic dependencies are detected and reported as error.
    */
   class ProjectPhases {
      /** @type {Map<string,BuildPhase>} Maps name (not full path) to BuildPhase. */
      phases
      /** @type {BuildProject} owner of this object */
      project

      ///////////////////////////////
      // Overwrite DependencyGraph //
      ///////////////////////////////
      /** @param {BuildProject} project @param {JSPO_Project_Definition} phasesDefinition @param {()=>ErrorContext} getEC */
      constructor(project, phasesDefinition, getEC) {
         typeof phasesDefinition === "object" || expectTypeError(phasesDefinition,"object",getEC())
         Array.isArray(phasesDefinition?.phases) || expectTypeError(phasesDefinition?.phases,"Array",getEC().newAccessPath("phases"))
         this.phases = new Map()
         this.project = project
         phasesDefinition.phases.forEach( (phaseDef, i) => {
            this.addPhase(phaseDef, getEC().newAccessPath("phases",i))
         })
      }

      /** @param {ErrorContext} ec @return {void} */
      validateSync(ec) {
         DependencyGraph.validate(DependencyGraph.getClosure([...this.phases.values()]),BuildPhase.TYPENAME,ec)
         for (const phase of this.phases.values())
            phase.validatePrePost(ec)
      }

      /** @param {ErrorContext} ec */
      async validate(ec) {
         for (const phase of this.phases.values())
            await phase.validate(ec)
      }

      ///////////

      /** @return {BuildConfig} */
      get config() { return this.project.config }

      /** @return {SharedConfigs} */
      get sharedConfigs() { return this.project.projects.sharedConfigs }

      /** @return {SharedPhases} */
      get sharedPhases() { return this.project.projects.sharedPhases }

      /** @type {string[]} */
      get phaseNames() { return [...this.phases.keys()] }

      /** @param {string} name @param {()=>ErrorContext} getEC @return {BuildPhase} */
      phase(name, getEC) { return this.phases.get(name) ?? error(`Phase ${getEC().ap()}=${JSON.stringify(name)} does not exist.`,getEC()) }

      ///////////

      /** @param {string|JSPO_PhaseRef_Definition} phaseRef @param {()=>ErrorContext} getEC @return {{project:string, config:BuildConfig, phase:string}} */
      validatePhaseRef(phaseRef, getEC) {
         const result = { project:this.project.name, config:this.config, phase:"" }
         getEC ??= () => new ErrorContext({fct:"validatePhaseRef(p)",accesspath:["p"]})
         typeof phaseRef === "string" || typeof phaseRef === "object" || expectTypeError(phaseRef,"string|object",getEC())
         if (typeof phaseRef === "string") {
            validateName(phaseRef, getEC)
            result.phase = phaseRef
         }
         else {
            validateName(phaseRef.phase, () => getEC().newAccessPath("phase"))
            phaseRef.project === undefined || validateName(phaseRef.project, () => getEC().newAccessPath("project"))
            phaseRef.config === undefined || validateName(phaseRef.config, () => getEC().newAccessPath("config"))
            const buildConfig = this.sharedConfigs.config(phaseRef.config, () => getEC().newAccessPath("config"))
            result.phase = phaseRef.phase
            result.config = buildConfig
            result.project = phaseRef.project ?? this.project.name
         }
         return result
      }

      /** @param {JSPO_Phase_Definition} phaseDefinition @param {ErrorContext} [ec] */
      addPhase(phaseDefinition, ec) {
         const getEC0 = () => ec?.newProject(this.project.name) ?? new ErrorContext({fct:"addPhase(p)",accesspath:["p"],project:this.project.name})
         typeof phaseDefinition === "object" || expectTypeError(phaseDefinition,"object",getEC0())
         const phaseName = phaseDefinition.phase
         validateName(phaseName, () => getEC0().newAccessPath("phase"))
         this.phases.has(phaseName) && error(`Expect ${getEC0().ap("phase")}=${JSON.stringify(phaseName)} to be unique.`,getEC0())
         const getEC = () => getEC0().newPhase(phaseName)
         const prePhaseRef = phaseDefinition.prePhase === undefined ? undefined : this.validatePhaseRef(phaseDefinition.prePhase, () => getEC().newAccessPath("prePhased"))
         const postPhaseRef = phaseDefinition.postPhase === undefined ? undefined : this.validatePhaseRef(phaseDefinition.postPhase, () => getEC().newAccessPath("postPhase"))
         const dependsOnPhaseRef = []
         if (phaseDefinition.dependsOnPhases) {
            Array.isArray(phaseDefinition.dependsOnPhases) || expectTypeError(phaseDefinition.dependsOnPhases,"Array",getEC().newAccessPath("dependsOnPhases"))
            dependsOnPhaseRef.push(...phaseDefinition.dependsOnPhases.map( (phaseRef, i) => this.validatePhaseRef(phaseRef,() => getEC().newAccessPath("dependsOnPhases",i))))
         }
         phaseDefinition.builds === undefined || Array.isArray(phaseDefinition.builds) || expectTypeError(phaseDefinition.builds,"Array",getEC().newAccessPath("builds"))
         const newPhase = this.sharedPhases.ensureNode(this.project.name,this.config,phaseName), createdFrom = newPhase
         /** @param {{project:string,config:BuildConfig,phase:string}} ref @return {BuildPhase} */
         const getPhase = ({project,config,phase}) => this.sharedPhases.ensureNode(project, config, phase, createdFrom)
         const dependsOnPhases = dependsOnPhaseRef.map( (ref) => getPhase(ref))
         const prePhase = prePhaseRef && getPhase(prePhaseRef)
         const postPhase = postPhaseRef && getPhase(postPhaseRef)
         phaseDefinition.builds?.forEach( (build, i) => newPhase.addBuild(build,getEC().newAccessPath("builds",i)))
         this.phases.set(phaseName, newPhase.init({project:this.project,comment:phaseDefinition.comment,prePhase,postPhase,dependentOn:dependsOnPhases,ec:getEC()}))
         this.validateSync(getEC())
      }
   }

   class BuildProject {
      /** @type {BuildProjects} Owner of this object. */
      projects
      /** @type {string} */
      name
      /** @type {string} */
      comment
      /** @type {BuildConfig} */
      config
      /** @type {ProjectPhases} */
      phases

      /** @param {BuildProjects} projects @param {JSPO_Project_Definition} definition @param {()=>ErrorContext} getEC */
      constructor(projects, definition, getEC) {
         typeof definition === "object" || expectTypeError(definition,"object",getEC())
         const name = definition?.project
         validateName(name, () => getEC().newAccessPath("project"))
         const ct = typeof definition.config
         ct === "undefined" || ct === "string" || ct === "object" || expectTypeError(ct,"string|object|undefined",getEC().newAccessPath("config"))
         const getEC2 = () => getEC().newProject(this.name)
         this.projects = projects
         this.name = name
         this.comment = String(definition.comment??"").trim()
         this.config = typeof definition.config === "object" ? new BuildConfig(projects.sharedConfigs,definition.config,()=>getEC().newAccessPath("config")) : projects.matchConfig(definition.config,()=>getEC2().newAccessPath("config"))
         typeof definition.config === "object" && projects.matchConfig(this.config.name,()=>getEC2().newAccessPath("config","config"))
         this.phases = new ProjectPhases(this, definition, () => getEC2().newConfig(this.config.name))
      }

      /** @type {string[]} */
      get phaseNames() { return this.phases.phaseNames }

      /** @param {string} name @param {()=>ErrorContext} [getEC] @return {BuildPhase} */
      phase(name, getEC) { return this.phases.phase(name,() => getEC?.().newProject(this.name) ?? new ErrorContext({fct:"phase(name)",accesspath:["name"],project:this.name})) }

      /** @param {JSPO_Phase_Definition} phaseDefinition */
      addPhase(phaseDefinition) { this.phases.addPhase(phaseDefinition) }

      /** @param {ErrorContext} [ec] @return {Promise<void>} */
      async validate(ec) { return this.phases.validate(ec?.newProject(this.name) ?? new ErrorContext({fct:"validate",project:this.name})) }

   }

   class BuildProjects {
      /** @type {BuildProject[]} */
      projects
      /** @type {Map<string,BuildProject>} */
      nameconfigIndex
      /** @type {Map<string,BuildProject[]>} */
      name2projects
      /** @type {Map<string,BuildProject[]>} */
      config2projects
      /** @type {SharedConfigs} */
      sharedConfigs
      /** @type {SharedPhases} */
      sharedPhases

      constructor() {
         this.projects = []
         this.nameconfigIndex = new Map()
         this.name2projects = new Map()
         this.config2projects = new Map()
         this.sharedConfigs = new SharedConfigs()
         this.sharedPhases = new SharedPhases()
      }

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) {
         ec ??= new ErrorContext({fct:"validate()"})
         for (const project of this.projects)
            await project.validate(ec)
         await this.sharedPhases.validate(ec)
      }

      ///////////

      /** @return {BuildConfig} */
      get defaultConfig() { return this.sharedConfigs.defaultConfig }

      /** @param {string} project @param {string} [config] @return {boolean} */
      has(project,config) {
         config ??= this.sharedConfigs.hasDefault ? this.sharedConfigs.defaultConfig.name : undefined
         return config !== undefined && this.nameconfigIndex.has(this.canonicalName(project,config))
      }
      /** @param {string} config @return {boolean} */
      hasProjectsWithConfig(config) { return this.config2projects.has(config??this.defaultConfig.name) }
      /** @param {string} project @return {boolean} */
      hasProjectsWithName(project) { return this.name2projects.has(project) }

      /** @param {string} project @param {string} [config] @param {()=>ErrorContext} [getEC] @return {BuildProject} */
      project(project, config, getEC) {
         getEC ??= () => new ErrorContext({fct:"project(project,config)"})
         config = this.matchConfig(config,()=>getEC().newAccessPath("config")).name
         return this.nameconfigIndex.get(this.canonicalName(project,config)) ?? error(`Unknown project ${getEC().ap("project")}=${JSON.stringify(project)} ${getEC().ap("config")}=${JSON.stringify(config)} combination.`,getEC())
      }

      /** @param {string} [config] @param {()=>ErrorContext} [getEC] @return {BuildProject[]} */
      projectsByConfig(config, getEC) {
         getEC ??= () => new ErrorContext({fct:"projectsByConfig(config)",accesspath:["config"]})
         config = this.matchConfig(config,getEC).name
         return [...(this.config2projects.get(config)??[])]
      }

      /** @param {string} project @param {string} config */
      canonicalName(project,config) { return JSON.stringify(project)+","+JSON.stringify(config) }

      /** @param {undefined|string} config @param {()=>ErrorContext} getEC @return {BuildConfig} */
      matchConfig(config, getEC) { return this.sharedConfigs.config(config,getEC) }

      ///////////

      /** @param {JSPO_Project_Definition} definition @param {()=>ErrorContext} [getEC] */
      addProject(definition, getEC) {
         getEC ??= () => new ErrorContext({fct:"addProject(p)",accesspath:["p"]})
         const newProject = new BuildProject(this, definition, getEC)
         const name = newProject.name, config = newProject.config.name
         this.has(name,config) && error(`Expect unique combination ${getEC().ap("project")}=${JSON.stringify(definition.project)} ${getEC().ap("config")}=${JSON.stringify(definition.config)}`,getEC().newProject(name).newConfig(config))
         this.projects.push(newProject)
         this.nameconfigIndex.set(this.canonicalName(name,config),newProject)
         this.name2projects.get(name)?.push(newProject) ?? this.name2projects.set(name, [newProject])
         this.config2projects.get(config)?.push(newProject) ?? this.config2projects.set(config, [newProject])
      }
   }

   class BuildConfig {
      /** @type {SharedConfigs} Owner of this object */
      sharedConfigs
      /** @type {string} */
      name
      /** @type {JSPO_ProjectConfig_Definition} */
      values

      /** @param {SharedConfigs} sharedConfigs @param {JSPO_ProjectConfig_Definition} definition @param {()=>ErrorContext} getEC */
      constructor(sharedConfigs, definition, getEC) {
         typeof definition === "object" || expectTypeError(definition,"object",getEC())
         const name = definition?.config
         validateName(name, () => getEC().newAccessPath("config"))
         this.sharedConfigs = sharedConfigs
         this.name = name
         this.values = definition
      }

      ///////////

      /** @return {boolean} */
      get isDefault() { return this.sharedConfigs.defaultConfig.name === this.name }
   }

   class SharedConfigs {
      /** @type {Map<string,BuildConfig>} */
      configs = new Map()
      /** @type {undefined|BuildConfig} */
      default

      constructor() {
         this.sharedPhases = new SharedPhases()
      }

      ///////////

      /** @return {string[]} */
      get configNames() { return [...this.configs.keys()] }
      /** @return {BuildConfig} */
      get defaultConfig() { return this.default ?? error(`Unknown default configuration (call setDefaultConfig before).`,new ErrorContext({fct:"defaultConfig()"})) }
      /** @return {boolean} */
      get hasDefault() { return this.default !== undefined }

      /** @param {null|undefined|string} name @param {()=>ErrorContext} getEC @return {BuildConfig} */
      config(name, getEC) {
         return name == null
                  ? this.default ?? error(`Unknown default configuration (call setDefaultConfig before).`,getEC())
                  : this.configs.get(name) ?? error(`Unknown configuration ${getEC().ap()}=${JSON.stringify(name)}.`,getEC())
      }

      /** @param {string} name @return {boolean} */
      has(name)  { return this.configs.has(name) }

      ///////////

      /** @param {...JSPO_ProjectConfig_Definition} configs */
      addConfigs(...configs) {
         const getEC = () => new ErrorContext({fct:"addConfigs(p)",accesspath:["p"]})
         Array.isArray(configs) || expectTypeError(configs,"Array",getEC())
         configs.forEach( (definition, i) => {
            const newConfig = new BuildConfig(this, definition, () => getEC().newAccessPath(i))
            this.configs.set(newConfig.name,newConfig)
         })
      }

      /** @param {JSPO_ProjectConfig_Definition} definition */
      setDefaultConfig(definition) {
         const getEC = () => new ErrorContext({fct:"setDefaultConfig(p)",accesspath:["p"]})
         this.default && error(`Can not overwrite default configuration with p.config=${JSON.stringify(definition?.config)}.`,getEC().newConfig(this.default.name))
         this.default = new BuildConfig(this, definition, getEC)
         this.configs.set(this.default.name,this.default)
      }
   }

   class SharedPhases extends DependencyGraph {
      /** @type {Map<string,BuildPhase>}  */
      nodes

      constructor() {
         super()
         this.nodes = new Map()
      }

      /** @param {ErrorContext} ec @return {Promise<void>} */
      async validate(ec) {
         // !! should be catched by project.validate !!
         DependencyGraph.validate(this.nodes,BuildPhase.TYPENAME,ec)
      }

      /** @param {BuildPhase} node @return {TProcessingStep<BuildPhase>} */
      static newProcessingStep(node) { return ProcessingStep.create(node) }

      /** @param {number} level @param {ErrorContext} ec @param {Map<string,BuildPhase>} subset @return {Promise<TProcessingStep<BuildPhase>[]>} */
      async getProcessingSteps(level, ec, subset) {
         return DependencyGraph.getProcessingSteps(subset ?? this.nodes, BuildPhase.TYPENAME, level, SharedPhases.newProcessingStep, ec)
      }

      ///////////

      /** @param {string} project @param {BuildConfig} config @param {string} phase @param {BuildPhase} [createdFrom] */
      ensureNode(project, config, phase, createdFrom) { return DependencyGraph.ensureNode(BuildPhase.path(project,config,phase), this.nodes, () => new BuildPhase(project,config,phase,createdFrom)) }

   }

   class BuildTargets {
      /** @type {Map<string,JSPO_Target_Definition>} */
      targets = new Map()

      ///////////

      /** @return {string[]} */
      get targetNames() { return [...this.targets.keys()] }

      /** @param {string} name @return {JSPO_Target_Definition} */
      target(name) { return this.targets.get(name) || error(`Target »${String(name)}« is not defined.`,new ErrorContext({fct:"target(name)"})) }

      ///////////

      /** @param {JSPO_Target_Definition} target @param {()=>ErrorContext} [getEC] */
      addTarget(target, getEC) {
         getEC ??= () => new ErrorContext({fct:"addTarget(p)",accesspath:["p"]})
         typeof target === "object" || expectTypeError(target,"object",getEC())
         const name = target?.target
         validateName(name, () => getEC().newAccessPath("target"))
         const comment = target.comment != null ? String(target.comment).trim() : ""
         Array.isArray(target.builds) || expectTypeError(target.builds,"Array",getEC().newAccessPath("builds"))
         target.builds.forEach( (build,i) => {
            typeof build === "object" || expectTypeError(build,"object",getEC().newAccessPath("builds",i))
            typeof build.project === "string" || expectTypeError(build.project,"string",getEC().newAccessPath("builds",i,"project"))
            build.config === undefined || typeof build.config === "string" || expectTypeError(build.config,"string",getEC().newAccessPath("builds",i,"config"))
            typeof build.phase === "string" || expectTypeError(build.phase,"string",getEC().newAccessPath("builds",i,"phase"))
            build.build === undefined || typeof build.build === "string" || expectTypeError(build.build,"string",getEC().newAccessPath("builds",i,"build"))
         })
         this.targets.set(name, { target:target.target, comment, builds:target.builds.map(b=>({project:b.project,config:b.config,phase:b.phase,build:b.build})) })
      }

      /** @param {JSPO_Target_Definition[]} targets */
      addTargets(targets) {
         const getEC = () => new ErrorContext({fct:"addTargets(p)",accesspath:["p"]})
         Array.isArray(targets) || expectTypeError(targets,"Array",getEC())
         targets.forEach( (target, i) => {
            this.addTarget(target,() => getEC().newAccessPath(i))
         })
      }
   }

   return new JSPO()
})()

/**
 * @typedef Debug_ProjectConfig
 * @property {"debug"} config
 * @property {string} outdir
 * @property {true} debug
 * @property {false} release
 * @property {false} test
 *
 * @typedef Release_ProjectConfig
 * @property {"release"} config
 * @property {string} outdir
 * @property {false} debug
 * @property {true} release
 * @property {false} test
 *
 * @typedef Test_ProjectConfig
 * @property {"test"} config
 * @property {string} outdir
 * @property {false} debug
 * @property {false} release
 * @property {true} test
 *
 * @typedef Default_ProjectConfig
 * @property {"default"} config
 * @property {string} outdir
 * @property {true} default
 * @property {false} debug
 * @property {false} release
 * @property {false} test
 *
 * @typedef {Default_ProjectConfig|Debug_ProjectConfig|Release_ProjectConfig|Test_ProjectConfig} JSPO_ProjectConfig
 */

jspo.setDefaultConfig({ config: "default", outdir:"bin", default: true, debug:false, release:false, test:false })

jspo.addConfigs(
   { config: "debug", outdir: "bin/debug", debug:true, release:false, test:false },
   { config: "release", outdir: "bin/release", debug:false, release:true, test:false },
   { config: "test", outdir: "bin/test", debug:false, release:false, test:true },
)

jspo.addTargets(
   {  target: "clean",
      comment: "Removes all build artefacts",
      builds: [
         { project: "default", phase:"clean" },
      ]},
   {  target: "build",
      comment: "Matches top-level-project with a build phase",
      builds: [
         { project: "default", config:"release", phase:"build-javascript-modules" },
         { project: "project-with-cycles-1", phase:"build" },
         { project: "p1", phase:"build" },
      ]},
)

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
      { phase: "clean", prePhase: "pre-clean", postPhase: "post-clean" },
      { phase: "build-javascript-modules", dependsOnPhases: ["clean",{project:"build-javascript",phase:"build"},{config:"debug",phase:"xxx"}] },
   ]
})

console.log("====\nTEST jspo.forEachConfig\n====")
jspo.forEachConfig( (cfg) => {
   /** @type {undefined|JSPO_ProjectConfig} */
   const config = ({
      debug: { ...jspo.config("debug").values, outdir: "bin/project/default/debug/" },
      release: { ...jspo.config("release").values, outdir: "bin/project/default/release" },
   })[cfg.name] || undefined
   if (config === undefined) return
   console.log(`add project:default config:${cfg.name}`)
   jspo.addProject({
      project: "default",
      config: config,
      phases: [
         { phase: "xxx", prePhase: {phase:"pre-clean"}, postPhase: {project:"default",config:"default",phase:"post-clean"},
         builds: [
                     // files xxx2 and xxx3 must pre exist (no build rule) else build error (use touch xxx2 / xxx3)
                     { file:"xxx1", dependsOnBuilds:["xxx2","xxx3"], action: async ()=>console.log("build xxx1") },
                     { file:"xxx10", dependsOnBuilds:["xxx1","xxx3"], action: async ()=>console.log("build xxx10") },
                     { command:`xxx`, action:async (arg)=>console.log(`xxx commands for config: ${arg.config} and outdir: ${config.outdir}`) },
                  ]
         },
         { phase: "build-javascript-modules", dependsOnPhases: [{phase:"clean"},{project:"build-javascript",phase:"build"},"xxx"] },
      ]
   })
})
console.log()


// jspo.addProject({project:"cycle",phases:[{phase:"dep1", dependsOnProjects:[{project:"default",phase:"dep1"}]}]})
// jspo.project("default").addPhase({phase:"dep1", dependsOnProjects:[{project:"cycle",phase:"dep1"}]})
// await jspo.project("default").validate()

// project: "default" && phase: "build-javascript-modules"
jspo.addBuild("default", undefined, "build-javascript-modules", { file:"test.mjs", dependsOnBuilds:["html-snippets/test.js"], action: async ({ec}) => {
   console.log("Build test.mjs")
   await jspo.files.overwrite("test.mjs",
      jspo.appendESModuleExport(
            await jspo.files.loadString("html-snippets/test.js",ec),
            [ "TEST", "RUN_TEST", "SUB_TEST", "END_TEST", "RESET_TEST", "NEW_TEST" ]
      ),
   )
}})
jspo.addBuild("default", undefined, "build-javascript-modules", { command:"test.mjs postprocessor", dependsOnBuilds:["test.mjs"], action:async ()=>console.log("test.mjs postprocessor") })

const cleanPhase = jspo.phase("default",undefined,"clean")
cleanPhase.addBuild({ command:"remove-files", dependsOnBuilds:[], action:async ()=>console.log("remove files") })
cleanPhase.addBuild({ command:"remove-dirs", dependsOnBuilds:["remove-files"], action:async ()=>console.log("remove directories") })
jspo.phase("default",undefined,"pre-clean").addBuild({ command:"pre-clean", action:async ()=>console.log("pre-clean commands") })
jspo.phase("default",undefined,"post-clean").addBuild({ command:"post-clean", action:async ()=>console.log("post-clean commands") })

await jspo.validateProjects()

console.log("====\nTEST cleanPhase.build()\n====\n")

await cleanPhase.build()

console.log(`====\nTEST jspo.build("default",undefined,"build-javascript-modules")\n====\n`)

await jspo.buildSteps("default", undefined, "build-javascript-modules")
   // .then( steps => console.log(steps))
   .then( () => jspo.build("default", undefined, "build-javascript-modules"))
   .catch( e => console.log("error",e))


// uncomment cycles for following test
console.log("====\nTEST jspo.validateProjects()\n====\n")

jspo.addProject({
   project: "project-with-cycles-1",
   phases: [ { phase: "build", dependsOnPhases:true?[]:[{project:"project-with-cycles-2",phase:"build"}]} ]
})

jspo.addProject({
   project: "project-with-cycles-2",
   phases: [ { phase: "build", dependsOnPhases:[{project:"project-with-cycles-1",phase:"build"}],
               builds: [
                  { command:"c1", dependsOnBuilds:["c2"], action: async ()=>console.log("build c1") },
                  { command:"c2", dependsOnBuilds:["c3"], action: async ()=>console.log("build c2") },
                  { command:"c3", dependsOnBuilds:["c4"], action: async ()=>console.log("build c3") },
                  { command:"c4", dependsOnBuilds:true?[]:["c2"], action: async ()=>console.log("build c4") },
               ] } ]
})

await jspo.validateProjects().catch(e => {
   console.log("\n"+String(e))
})

console.log("====\nTEST jspo.build('project-with-cycles-2',undefined,'build',['c4'])\n     condition: subset of nodes\n====\n")

await jspo.build("project-with-cycles-2",undefined,"build",["c4"])


console.log("====\nTEST jspo.build('p1',undefined,'build')\n     condition: project p3 is undefined and command c3 also\n====\n")

jspo.addProject({
   project: "p1",
   phases: [ { phase: "build", dependsOnPhases:[{project:"p2",phase:"build"}]} ]
})

jspo.addProject({
   project: "p2",
   phases: [ { phase: "build", dependsOnPhases:[{project:"p3",phase:"build"}]} ]
})

jspo.project("p1").phase("build").addBuild({command:"c1",dependsOnBuilds:["c2"],action:async()=>{console.log("project p1 command c1")}})
jspo.project("p1").phase("build").addBuild({command:"c2",dependsOnBuilds:["c3"],action:async()=>{console.log("project p1 command c2")}})
// no validation allows to build "invalid" projects (cycles always prevent builds)
await jspo.project("p1").phase("build").build()


console.log("====\nTEST jspo CLI\n====")

jspo.commandLineProcessor.printUsage()

await jspo.processCommandLine()