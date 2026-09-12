import groovy.json.JsonOutput
import groovy.json.JsonSlurper
import org.gradle.api.GradleException
import org.gradle.api.tasks.Delete
import org.gradle.api.tasks.Sync
import org.gradle.api.tasks.bundling.Zip
import java.io.File
import java.io.InputStream
import java.security.MessageDigest
import java.util.zip.ZipFile

plugins {
    id("com.android.application")
}

val deviceVariant =
    providers.gradleProperty("kiwi.deviceBuildType").orElse("debug").get().lowercase()
val deviceVariantTaskSuffixes = mapOf(
    "debug" to "Debug",
    "release" to "Release",
    "debugallroms" to "DebugAllRoms",
    "releaseallroms" to "ReleaseAllRoms",
    "debugnopakresource" to "DebugNoPakResource",
    "releasenopakresource" to "ReleaseNoPakResource",
)
if (deviceVariant !in deviceVariantTaskSuffixes) {
    throw GradleException(
        "kiwi.deviceBuildType must be one of " +
            "${deviceVariantTaskSuffixes.keys.joinToString()}, got '$deviceVariant'"
    )
}

val demoRomZip = rootProject.file(
    providers.gradleProperty("kiwi.demoRomZip")
        .orElse(
            "../../third_party/Kiwi-Machine-Workspace/zipped/nes/" +
                "Super Mario Bros. (World).zip"
        )
        .get()
)
val generatedAssetsDirectory = layout.buildDirectory.dir("generated/demoAssets")
val generatedAllRomsAssetsDirectory = layout.buildDirectory.dir("generated/allRomsAssets")
val generatedAllRomsManifestsDirectory =
    layout.buildDirectory.dir("generated/allRomsManifests")
val generatedHdTextureAssetsDirectory =
    layout.buildDirectory.dir("generated/hdTextureAssets")
val allRomsDirectory = rootProject.file(
    providers.gradleProperty("kiwi.allRomsDirectory")
        .orElse("../../third_party/Kiwi-Machine-Workspace/zipped/nes")
        .get()
)
val hdTexturePaksDirectory = rootProject.file(
    providers.gradleProperty("kiwi.hdTexturePaksDirectory")
        .orElse("../../third_party/Kiwi-Machine-Workspace/out/textures")
        .get()
)

val packageDemoPak = tasks.register<Zip>("packageDemoPak") {
    doFirst {
        if (!demoRomZip.isFile) {
            throw GradleException(
                "Demo ROM package is missing: ${demoRomZip.absolutePath}\n" +
                    "Set kiwi.demoRomZip in gradle.properties to a valid game zip."
            )
        }
    }

    from(demoRomZip)
    from(layout.projectDirectory.file("src/demoPak/manifest.json"))
    archiveFileName.set("demo.pak")
    destinationDirectory.set(generatedAssetsDirectory)
    isPreserveFileTimestamps = false
    isReproducibleFileOrder = true
}

val cleanAllRomsPaks = tasks.register<Delete>("cleanAllRomsPaks") {
    delete(generatedAllRomsAssetsDirectory)
    delete(generatedAllRomsManifestsDirectory)
}

val allRomsPackageSources = mutableListOf(allRomsDirectory to "main.pak")
allRomsDirectory.listFiles()
    ?.filter { it.isDirectory }
    ?.sortedBy { it.name }
    ?.forEach { allRomsPackageSources.add(it to "${it.name}.pak") }

fun calculateSha1(input: InputStream): String {
    val digest = MessageDigest.getInstance("SHA-1")
    val buffer = ByteArray(64 * 1024)
    while (true) {
        val bytesRead = input.read(buffer)
        if (bytesRead < 0) {
            break
        }
        digest.update(buffer, 0, bytesRead)
    }
    return digest.digest().joinToString("") {
        "%02X".format(it.toInt() and 0xFF)
    }
}

fun collectRomSha1s(sourceDirectory: File): Map<String, Map<String, String>> {
    val packageHashes = linkedMapOf<String, Map<String, String>>()
    sourceDirectory.listFiles { file -> file.isFile && file.extension == "zip" }
        ?.sortedBy { it.name }
        ?.forEach { zipPath ->
            val romHashes = linkedMapOf<String, String>()
            ZipFile(zipPath).use { archive ->
                val entries = archive.entries()
                while (entries.hasMoreElements()) {
                    val entry = entries.nextElement()
                    if (entry.isDirectory || !entry.name.endsWith(".nes", ignoreCase = true)) {
                        continue
                    }
                    val romName = File(entry.name).nameWithoutExtension
                    archive.getInputStream(entry).use { input ->
                        romHashes[romName] = calculateSha1(input)
                    }
                }
            }
            if (romHashes.isEmpty()) {
                throw GradleException("No ROM files found in: ${zipPath.absolutePath}")
            }
            packageHashes[zipPath.nameWithoutExtension] = romHashes
        }
    return packageHashes
}

val allRomsPakTasks = allRomsPackageSources.mapIndexed { index, (sourceDirectory, packageName) ->
    val generatedManifest =
        generatedAllRomsManifestsDirectory.map { it.file("$index/manifest.json") }
    val generateManifest = tasks.register("generateAllRomsManifest$index") {
        dependsOn(cleanAllRomsPaks)

        val sourceManifest = sourceDirectory.resolve("manifest.json")
        inputs.file(sourceManifest)
        inputs.files(project.fileTree(sourceDirectory) { include("*.zip") })
        outputs.file(generatedManifest)

        doLast {
            val parsedManifest = JsonSlurper().parse(sourceManifest)
            if (parsedManifest !is MutableMap<*, *>) {
                throw GradleException(
                    "Package manifest is not a JSON object: ${sourceManifest.absolutePath}"
                )
            }
            @Suppress("UNCHECKED_CAST")
            val manifest = parsedManifest as MutableMap<String, Any?>
            manifest["rom_sha1s"] = collectRomSha1s(sourceDirectory)

            val output = generatedManifest.get().asFile
            output.parentFile.mkdirs()
            output.writeText(
                JsonOutput.prettyPrint(JsonOutput.toJson(manifest)),
                Charsets.UTF_8,
            )
        }
    }

    tasks.register<Zip>("packageAllRomsPak$index") {
        description = "Packages ${sourceDirectory.absolutePath} as $packageName."
        dependsOn(generateManifest)

        doFirst {
            if (!sourceDirectory.isDirectory) {
                throw GradleException(
                    "All-ROMs directory is missing: ${sourceDirectory.absolutePath}\n" +
                        "Set kiwi.allRomsDirectory in gradle.properties to the PC ROM package source."
                )
            }
            if (!sourceDirectory.resolve("manifest.json").isFile) {
                throw GradleException(
                    "Package manifest is missing: " +
                        sourceDirectory.resolve("manifest.json").absolutePath
                )
            }
            if (sourceDirectory.listFiles { file -> file.isFile && file.extension == "zip" }
                    .isNullOrEmpty()) {
                throw GradleException(
                    "No ROM zip files found in: ${sourceDirectory.absolutePath}"
                )
            }
        }

        from(sourceDirectory) {
            include("*.zip")
        }
        from(generatedManifest)
        archiveFileName.set(packageName)
        destinationDirectory.set(generatedAllRomsAssetsDirectory)
        isPreserveFileTimestamps = false
        isReproducibleFileOrder = true
    }
}

val packageAllRomsPaks = tasks.register("packageAllRomsPaks") {
    group = "build"
    description = "Packages every ROM set using the same layout as PC Kiwi Machine."
    dependsOn(allRomsPakTasks)
}

val stageHdTexturePaks = tasks.register<Sync>("stageHdTexturePaks") {
    group = "build"
    description = "Stages prebuilt HD texture PAKs under assets/textures."

    doFirst {
        val texturePaks =
            hdTexturePaksDirectory.listFiles { file ->
                file.isFile && file.extension.equals("pak", ignoreCase = true)
            }
        if (!hdTexturePaksDirectory.isDirectory || texturePaks.isNullOrEmpty()) {
            throw GradleException(
                "HD texture PAKs are missing: ${hdTexturePaksDirectory.absolutePath}\n" +
                    "Run the package_manager auto_package target first or set " +
                    "kiwi.hdTexturePaksDirectory."
            )
        }
    }

    from(hdTexturePaksDirectory) {
        include("*.pak")
    }
    into(generatedHdTextureAssetsDirectory.map { it.dir("textures") })
}

val validateCachedPakResources = tasks.register("validateCachedPakResources") {
    group = "verification"
    description = "Checks cached ROM and HD texture PAKs without regenerating them."

    doLast {
        val cachedPackages = generatedAllRomsAssetsDirectory.get().asFile
        val cachedTextures =
            generatedHdTextureAssetsDirectory.get().asFile.resolve("textures")
        val missingPackages =
            listOf("main.pak", "specials.pak").filterNot {
                cachedPackages.resolve(it).isFile
            }
        val texturePaks =
            cachedTextures.listFiles { file ->
                file.isFile && file.extension.equals("pak", ignoreCase = true)
            }
        if (missingPackages.isNotEmpty() || texturePaks.isNullOrEmpty()) {
            throw GradleException(
                "Cached PAK resources are unavailable. Build debugAllRoms or " +
                    "releaseAllRoms once before using a NoPakResource variant."
            )
        }
    }
}

android {
    namespace = "com.yuyisi.kiwimachine"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.yuyisi.kiwimachine"
        minSdk = 24
        targetSdk = 33
        versionCode = 1
        versionName = "2.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
            }
        }
    }

    buildTypes {
        debug {
            isDebuggable = true
            isJniDebuggable = true
        }
        release {
            isDebuggable = false
            isJniDebuggable = false
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")

            // Make the selected release variant installable on a local device.
            // Production signing remains untouched unless release is selected.
            if (deviceVariant == "release") {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
        create("debugAllRoms") {
            initWith(getByName("debug"))
            matchingFallbacks += listOf("debug")
        }
        create("releaseAllRoms") {
            initWith(getByName("release"))
            matchingFallbacks += listOf("release")
            signingConfig = signingConfigs.getByName("debug")
            if (deviceVariant == "releaseallroms") {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
        create("debugNoPakResource") {
            initWith(getByName("debug"))
            matchingFallbacks += listOf("debug")
        }
        create("releaseNoPakResource") {
            initWith(getByName("release"))
            matchingFallbacks += listOf("release")
            if (deviceVariant == "releasenopakresource") {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
    }
    sourceSets {
        getByName("debug").assets.srcDir(generatedAssetsDirectory)
        getByName("debug").assets.srcDir(generatedHdTextureAssetsDirectory)
        getByName("release").assets.srcDir(generatedAssetsDirectory)
        getByName("release").assets.srcDir(generatedHdTextureAssetsDirectory)
        getByName("debugAllRoms").assets.srcDir(generatedAllRomsAssetsDirectory)
        getByName("debugAllRoms").assets.srcDir(generatedHdTextureAssetsDirectory)
        getByName("releaseAllRoms").assets.srcDir(generatedAllRomsAssetsDirectory)
        getByName("releaseAllRoms").assets.srcDir(generatedHdTextureAssetsDirectory)
        getByName("debugNoPakResource").assets.srcDir(generatedAllRomsAssetsDirectory)
        getByName("debugNoPakResource").assets.srcDir(generatedHdTextureAssetsDirectory)
        getByName("releaseNoPakResource").assets.srcDir(generatedAllRomsAssetsDirectory)
        getByName("releaseNoPakResource").assets.srcDir(generatedHdTextureAssetsDirectory)
    }
    androidResources {
        noCompress += "pak"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures {
        viewBinding = true
    }
    ndkVersion = "26.1.10909125"
}

tasks.matching {
    (it.name.startsWith("merge") && it.name.endsWith("Assets")) ||
        it.name.startsWith("lint")
}.configureEach {
    if (name.contains("NoPakResource")) {
        dependsOn(validateCachedPakResources)
    } else {
        dependsOn(stageHdTexturePaks)
        if (name.contains("AllRoms")) {
            dependsOn(packageAllRomsPaks)
        } else {
            dependsOn(packageDemoPak)
        }
    }
}

val selectedDeviceVariant = deviceVariantTaskSuffixes.getValue(deviceVariant)

tasks.register("assembleDevice") {
    group = "build"
    description = "Assembles the selected device variant ($deviceVariant)."
    dependsOn("assemble$selectedDeviceVariant")
}

tasks.register("installDevice") {
    group = "install"
    description = "Builds and installs the selected device variant ($deviceVariant)."
    dependsOn("install$selectedDeviceVariant")
}

dependencies {

    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.6.1")
}
