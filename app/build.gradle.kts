plugins {
    id("com.android.application")
}

android {
    namespace = "com.populationmisfits.vr"
    compileSdk = 34
    ndkVersion = "26.3.11579264"

    defaultConfig {
        applicationId = "com.populationmisfits.vr"
        minSdk = 29
        targetSdk = 32
        versionCode = 100
        versionName = "1.0.0-v1"

        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-O3", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-missing-field-initializers")
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }

    buildFeatures {
        prefab = true
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
        }
        debug {
            isJniDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

dependencies {
    implementation("org.khronos.openxr:openxr_loader_for_android:1.1.38")
}
