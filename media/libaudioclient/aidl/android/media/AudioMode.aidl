/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.media;

/**
 * {@hide}
 */
@Backing(type="int")
enum AudioMode {
    INVALID = -2,
    CURRENT = -1,
    NORMAL = 0,
    RINGTONE = 1,
    IN_CALL = 2,
    IN_COMMUNICATION = 3,
    CALL_SCREEN = 4,
    FM = 5,
    MODE_FACTORY_TEST = 6,
}
