# Springboot autoConfig实现

## @Import注解

主要用于导入的一个或多个组件类——通常是@Configuration类,将其纳入spring容器中管理.

### 注解解析

当刷新容器时,会调用`invokeBeanFactoryPostProcessors`方法,转而进入`ConfigurationClassPostProcessor#postProcessBeanDefinitionRegistry`方法,该方法中会调用`processConfigBeanDefinitions`.这个方法用于建立 configuration关系并注册.在解析时会调用`ConfigurationClassParser`去解析注解及相关信息.

```java
//该方法会对配置类上的注解进行解析
protected final SourceClass doProcessConfigurationClass(
      ConfigurationClass configClass, SourceClass sourceClass, Predicate<String> filter)
      throws IOException {

   if (configClass.getMetadata().isAnnotated(Component.class.getName())) {
      // Recursively process any member (nested) classes first
      processMemberClasses(configClass, sourceClass, filter);
   }

   // Process any @PropertySource annotations
   for (AnnotationAttributes propertySource : AnnotationConfigUtils.attributesForRepeatable(
         sourceClass.getMetadata(), PropertySources.class,
         org.springframework.context.annotation.PropertySource.class)) {
      if (this.environment instanceof ConfigurableEnvironment) {
         processPropertySource(propertySource);
      }
      else {
         logger.info("Ignoring @PropertySource annotation on [" + sourceClass.getMetadata().getClassName() +
               "]. Reason: Environment must implement ConfigurableEnvironment");
      }
   }

   // Process any @ComponentScan annotations
   Set<AnnotationAttributes> componentScans = AnnotationConfigUtils.attributesForRepeatable(
         sourceClass.getMetadata(), ComponentScans.class, ComponentScan.class);
   if (!componentScans.isEmpty() &&
         !this.conditionEvaluator.shouldSkip(sourceClass.getMetadata(), ConfigurationPhase.REGISTER_BEAN)) {
      for (AnnotationAttributes componentScan : componentScans) {
         // The config class is annotated with @ComponentScan -> perform the scan immediately
         Set<BeanDefinitionHolder> scannedBeanDefinitions =
               this.componentScanParser.parse(componentScan, sourceClass.getMetadata().getClassName());
         // Check the set of scanned definitions for any further config classes and parse recursively if needed
         for (BeanDefinitionHolder holder : scannedBeanDefinitions) {
            BeanDefinition bdCand = holder.getBeanDefinition().getOriginatingBeanDefinition();
            if (bdCand == null) {
               bdCand = holder.getBeanDefinition();
            }
            if (ConfigurationClassUtils.checkConfigurationClassCandidate(bdCand, this.metadataReaderFactory)) {
               parse(bdCand.getBeanClassName(), holder.getBeanName());
            }
         }
      }
   }

   // 处理 @Import 注解,会将配置了改注解的类添加到spring生命周期中
   processImports(configClass, sourceClass, getImports(sourceClass), filter, true);

   // Process any @ImportResource annotations
   AnnotationAttributes importResource =
         AnnotationConfigUtils.attributesFor(sourceClass.getMetadata(), ImportResource.class);
   if (importResource != null) {
      String[] resources = importResource.getStringArray("locations");
      Class<? extends BeanDefinitionReader> readerClass = importResource.getClass("reader");
      for (String resource : resources) {
         String resolvedResource = this.environment.resolveRequiredPlaceholders(resource);
         configClass.addImportedResource(resolvedResource, readerClass);
      }
   }

   // Process individual @Bean methods
   Set<MethodMetadata> beanMethods = retrieveBeanMethodMetadata(sourceClass);
   for (MethodMetadata methodMetadata : beanMethods) {
      configClass.addBeanMethod(new BeanMethod(methodMetadata, configClass));
   }

   // Process default methods on interfaces
   processInterfaces(configClass, sourceClass);

   // Process superclass, if any
   if (sourceClass.getMetadata().hasSuperClass()) {
      String superclass = sourceClass.getMetadata().getSuperClassName();
      if (superclass != null && !superclass.startsWith("java") &&
            !this.knownSuperclasses.containsKey(superclass)) {
         this.knownSuperclasses.put(superclass, configClass);
         // Superclass found, return its annotation metadata and recurse
         return sourceClass.getSuperClass();
      }
   }

   // No superclass -> processing is complete
   return null;
}
```

@Important 主要用于引入实现了`ImportSelector`及`ImportBeanDefinitionRegistrar`接口的类,也可以支持自定义.

核心解析方法

```java
if (candidate.isAssignable(ImportSelector.class)) {
   // Candidate class is an ImportSelector -> delegate to it to determine imports
   Class<?> candidateClass = candidate.loadClass();
   ImportSelector selector = ParserStrategyUtils.instantiateClass(candidateClass, ImportSelector.class,
         this.environment, this.resourceLoader, this.registry);
   Predicate<String> selectorFilter = selector.getExclusionFilter();
   if (selectorFilter != null) {
      exclusionFilter = exclusionFilter.or(selectorFilter);
   }
   if (selector instanceof DeferredImportSelector) {
      this.deferredImportSelectorHandler.handle(configClass, (DeferredImportSelector) selector);
   }
   else {
      String[] importClassNames = selector.selectImports(currentSourceClass.getMetadata());
      Collection<SourceClass> importSourceClasses = asSourceClasses(importClassNames, exclusionFilter);
      processImports(configClass, currentSourceClass, importSourceClasses, exclusionFilter, false);
   }
}
else if (candidate.isAssignable(ImportBeanDefinitionRegistrar.class)) {
   // Candidate class is an ImportBeanDefinitionRegistrar ->
   // delegate to it to register additional bean definitions
   Class<?> candidateClass = candidate.loadClass();
   ImportBeanDefinitionRegistrar registrar =
         ParserStrategyUtils.instantiateClass(candidateClass, ImportBeanDefinitionRegistrar.class,
               this.environment, this.resourceLoader, this.registry);
   configClass.addImportBeanDefinitionRegistrar(registrar, currentSourceClass.getMetadata());
}
else {
   // Candidate class not an ImportSelector or ImportBeanDefinitionRegistrar ->
   // process it as an @Configuration class
   this.importStack.registerImport(
         currentSourceClass.getMetadata(), candidate.getMetadata().getClassName());
   processConfigurationClass(candidate.asConfigClass(configClass), exclusionFilter);
}
```

解析并注册 该注解下的类,如果是DeferredImportSelector则放到最后处理,如果不是则调用selectImports后再调用该方法,当作@Configuration 处理.

参考:[Spring中的@Import注解作用](https://blog.csdn.net/m0_49051691/article/details/108043511)

## @Enable* 的实现逻辑

@Enable注解依赖于 @Import注解导入类的实现.通同在刷新容器时将相关类注入到容器中.

## @Conditional 注解实现

执行过程与@Improt注解相似在调用`ConfigurationClassPostProcessor#postProcessBeanDefinitionRegistry`方法时,`ConfigurationClassBeanDefinitionReader#loadBeanDefinitions`加载configbean,该类中`loadBeanDefinitionsForBeanMethod`方法会做加载前校验.由`ConditionEvaluator#shouldSkip`方法完成.该方法中的`condition.matches`会调用 `OnBeanCondition#getMatchOutcome`方法校验bean是否需要被跳过.

注意:`ConditionalOn`注解处理在`@Import`之前,所以如果在config类中配置类同时用@Import引入类后会导致存在多个类实例,`@ConditionalOn*`失效.







参考:[@EnableAutoConfiguration处理逻辑](https://my.oschina.net/floor/blog/4354771)