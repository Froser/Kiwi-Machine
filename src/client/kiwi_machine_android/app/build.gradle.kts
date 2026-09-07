import org.gradle.api.GradleException
import org.gradle.api.tasks.bundling.Zip

plugins {
    id("com.android.application")
}

val deviceBuildType =
    providers.gradleProperty("kiwi.deviceBuildType").orElse("debug").get().lowercase()
if (deviceBuildType !in setOf("debug", "release")) {
    throw GradleException(
        "kiwi.deviceBuildType must be either 'debug' or 'release', got '$deviceBuildType'"
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
            if (deviceBuildType == "release") {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
    }
    sourceSets {
        getByName("main").assets.srcDir(generatedAssetsDirectory)
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
    dependsOn(packageDemoPak)
}

val selectedDeviceVariant =
    deviceBuildType.replaceFirstChar { if (it.isLowerCase()) it.titlecase() else it.toString() }

tasks.register("assembleDevice") {
    group = "build"
    description = "Assembles the selected device variant ($deviceBuildType)."
    dependsOn("assemble$selectedDeviceVariant")
}

tasks.register("installDevice") {
    group = "install"
    description = "Builds and installs the selected device variant ($deviceBuildType)."
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
