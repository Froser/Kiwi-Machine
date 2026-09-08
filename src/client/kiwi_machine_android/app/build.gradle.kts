import org.gradle.api.GradleException
import org.gradle.api.tasks.Delete
import org.gradle.api.tasks.bundling.Zip

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
val allRomsDirectory = rootProject.file(
    providers.gradleProperty("kiwi.allRomsDirectory")
        .orElse("../../third_party/Kiwi-Machine-Workspace/zipped/nes")
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
}

val allRomsPackageSources = mutableListOf(allRomsDirectory to "main.pak")
allRomsDirectory.listFiles()
    ?.filter { it.isDirectory }
    ?.sortedBy { it.name }
    ?.forEach { allRomsPackageSources.add(it to "${it.name}.pak") }

val allRomsPakTasks = allRomsPackageSources.mapIndexed { index, (sourceDirectory, packageName) ->
    tasks.register<Zip>("packageAllRomsPak$index") {
        description = "Packages ${sourceDirectory.absolutePath} as $packageName."
        dependsOn(cleanAllRomsPaks)

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
            include("manifest.json")
        }
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
            if (deviceVariant == "releaseallroms") {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
    }
    sourceSets {
        getByName("debug").assets.srcDir(generatedAssetsDirectory)
        getByName("release").assets.srcDir(generatedAssetsDirectory)
        getByName("debugAllRoms").assets.srcDir(generatedAllRomsAssetsDirectory)
        getByName("releaseAllRoms").assets.srcDir(generatedAllRomsAssetsDirectory)
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
    if (name.contains("AllRoms")) {
        dependsOn(packageAllRomsPaks)
    } else {
        dependsOn(packageDemoPak)
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
