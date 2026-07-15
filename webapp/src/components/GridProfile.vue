<template>
    <BootstrapAlert :show="!hasValidData">
        <h4 class="alert-heading"><BIconInfoSquare class="fs-2" />&nbsp;{{ $t('gridprofile.NoInfo') }}</h4>
        {{ $t('gridprofile.NoInfoLong') }}
    </BootstrapAlert>

    <template v-if="hasValidData">
        <table class="table table-hover">
            <tbody>
                <tr>
                    <td>{{ $t('gridprofile.Name') }}</td>
                    <td>{{ gridProfileList.name }}</td>
                </tr>
                <tr>
                    <td>{{ $t('gridprofile.Version') }}</td>
                    <td>{{ gridProfileList.version }}</td>
                </tr>
            </tbody>
        </table>

        <BootstrapAlert :show="true" variant="danger">
            <h4 class="info-heading"><BIconExclamationTriangle class="fs-2" />&nbsp;{{
                $t('gridprofile.WriteWarningTitle')
            }}</h4>
            <div v-html="$t('gridprofile.WriteWarningLong')"></div>
        </BootstrapAlert>

        <!-- Primary path: choose a verified profile from the list -->
        <div class="mb-3">
            <label class="form-label" for="gpPreset">{{ $t('gridprofile.ChooseProfile') }}</label>
            <select id="gpPreset" class="form-select" v-model="selectedPreset" @change="onSelectPreset">
                <option value="">{{ $t('gridprofile.PresetCurrent') }}</option>
                <option v-for="preset in presets" :key="preset.key" :value="preset.key">{{ preset.name }}</option>
            </select>
            <div class="form-text">{{ $t('gridprofile.ChooseProfileHint') }}</div>
        </div>

        <BootstrapAlert :show="presetLoaded" variant="warning">
            {{ $t('gridprofile.WillWritePreset', { name: selectedPresetName }) }}
        </BootstrapAlert>

        <!-- Decoded view of the profile currently on the inverter. Read-only unless
             manual editing has been explicitly enabled (and no preset is selected). -->
        <div class="accordion" id="accordionProfile">
            <div
                class="accordion-item accordion-table"
                v-for="(section, index) in gridProfileList.sections"
                :key="index"
            >
                <h2 class="accordion-header">
                    <button
                        class="accordion-button collapsed"
                        type="button"
                        data-bs-toggle="collapse"
                        :data-bs-target="`#collapse${index}`"
                        aria-expanded="true"
                        :aria-controls="`collapse${index}`"
                    >
                        {{ section.name }}
                        <span v-if="editing && sectionChangeCount(section) > 0" class="badge text-bg-warning ms-2">
                            {{ sectionChangeCount(section) }}
                        </span>
                    </button>
                </h2>
                <div :id="`collapse${index}`" class="accordion-collapse collapse" data-bs-parent="#accordionProfile">
                    <div class="accordion-body">
                        <table class="table table-hover">
                            <tbody>
                                <tr
                                    v-for="value in section.items"
                                    :key="value.n"
                                    :class="{ 'table-warning': editing && isChanged(value) }"
                                >
                                    <th>{{ value.n }}</th>
                                    <td>
                                        <!-- Editable boolean flag -->
                                        <template v-if="editing && value.o !== undefined && value.u == 'bool'">
                                            <div class="form-check form-switch mb-0">
                                                <input
                                                    class="form-check-input"
                                                    type="checkbox"
                                                    :checked="readValue(value) === 1"
                                                    @change="
                                                        writeValue(value, ($event.target as HTMLInputElement).checked ? 1 : 0)
                                                    "
                                                />
                                            </div>
                                        </template>

                                        <!-- Editable numeric value -->
                                        <template v-else-if="editing && value.o !== undefined">
                                            <div class="input-group input-group-sm gridprofile-value">
                                                <input
                                                    type="number"
                                                    class="form-control text-end"
                                                    :step="stepFor(value)"
                                                    :value="readValue(value)"
                                                    @change="onNumberInput(value, ($event.target as HTMLInputElement).value)"
                                                />
                                                <span class="input-group-text" v-if="value.u">{{ value.u }}</span>
                                            </div>
                                        </template>

                                        <!-- Read-only display -->
                                        <template v-else>
                                            <template v-if="value.u != 'bool'">
                                                {{ $n(value.v, 'decimal') }} {{ value.u }}
                                            </template>
                                            <StatusBadge
                                                v-else
                                                :status="value.v == 1"
                                                true_text="gridprofile.Enabled"
                                                false_text="gridprofile.Disabled"
                                            />
                                        </template>

                                        <div v-if="editing && isChanged(value)" class="form-text text-warning-emphasis mt-1">
                                            {{ $t('gridprofile.WasValue', { value: $n(value.v, 'decimal') }) }}
                                        </div>
                                    </td>
                                </tr>
                            </tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>

        <!-- Manual editing is hidden behind an explicit danger action -->
        <div class="mt-3 d-flex flex-wrap gap-2 align-items-center">
            <button
                v-if="!editMode && !presetLoaded"
                type="button"
                class="btn btn-outline-danger btn-sm"
                @click="enableEditMode()"
            >
                <BIconExclamationTriangle />&nbsp;{{ $t('gridprofile.EditManually') }}
            </button>
            <template v-if="editing">
                <button type="button" class="btn btn-outline-secondary btn-sm" @click="discardEdits()">
                    {{ $t('gridprofile.DiscardEdits') }}
                </button>
                <span v-if="changeCount > 0" class="badge text-bg-warning">
                    {{ $t('gridprofile.ChangedCount', { count: changeCount }) }}
                </span>
            </template>
        </div>

        <BootstrapAlert :show="editing" variant="danger" class="mt-2">
            {{ $t('gridprofile.ManualEditDanger') }}
        </BootstrapAlert>

        <div class="form-check mt-3 mb-2">
            <input class="form-check-input" type="checkbox" v-model="acknowledgeRisk" id="gpAckRisk" />
            <label class="form-check-label" for="gpAckRisk">
                {{ $t('gridprofile.AcknowledgeRisk') }}
            </label>
        </div>

        <BootstrapAlert v-model="showWriteAlert" :variant="writeAlertType" dismissible>
            <span v-if="writePolling" class="spinner-border spinner-border-sm me-2" role="status" aria-hidden="true"></span>
            {{ writeAlertMessage }}
        </BootstrapAlert>

        <button
            type="button"
            class="btn btn-danger"
            :disabled="!acknowledgeRisk || writeLoading"
            @click="onWriteGridProfile()"
        >
            <span v-if="writeLoading" class="spinner-border spinner-border-sm" role="status" aria-hidden="true"></span>
            {{ $t('gridprofile.WriteButton') }}
        </button>

        <br />
        <br />

        <div class="accordion" id="accordionAdvanced">
            <div class="accordion-item">
                <h2 class="accordion-header">
                    <button
                        class="accordion-button collapsed"
                        type="button"
                        data-bs-toggle="collapse"
                        data-bs-target="#collapseAdvanced"
                        aria-expanded="false"
                        aria-controls="collapseAdvanced"
                    >
                        {{ $t('gridprofile.AdvancedPaste') }}
                    </button>
                </h2>
                <div id="collapseAdvanced" class="accordion-collapse collapse" data-bs-parent="#accordionAdvanced">
                    <div class="accordion-body">
                        <div class="mb-2">
                            <label class="form-label" for="gpRawBytes">{{ $t('gridprofile.RawBytes') }}</label>
                            <textarea
                                id="gpRawBytes"
                                class="form-control font-monospace"
                                rows="5"
                                v-model="pasteRaw"
                            ></textarea>
                            <div class="form-text">{{ $t('gridprofile.RawBytesHint') }}</div>
                        </div>
                        <button type="button" class="btn btn-outline-primary btn-sm" @click="loadPaste()">
                            {{ $t('gridprofile.LoadPaste') }}
                        </button>

                        <hr />
                        <label class="form-label">{{ $t('gridprofile.RawPreview') }}</label>
                        <div><samp>{{ workingHex() }}</samp></div>
                    </div>
                </div>
            </div>
        </div>

        <br />

        <div class="accordion" id="accordionDev">
            <div class="accordion-item">
                <h2 class="accordion-header">
                    <button
                        class="accordion-button collapsed"
                        type="button"
                        data-bs-toggle="collapse"
                        data-bs-target="#collapseDev"
                        aria-expanded="true"
                        aria-controls="collapseDev"
                    >
                        {{ $t('gridprofile.GridprofileSupport') }}
                    </button>
                </h2>
                <div id="collapseDev" class="accordion-collapse collapse" data-bs-parent="#accordionDev">
                    <div class="accordion-body">
                        <BootstrapAlert :show="true" variant="danger">
                            <h4 class="info-heading">
                                <BIconInfoSquare class="fs-2" />&nbsp;{{ $t('gridprofile.GridprofileSupport') }}
                            </h4>
                            <div v-html="$t('gridprofile.GridprofileSupportLong')"></div>
                        </BootstrapAlert>
                        <samp>
                            {{ rawContent() }}
                        </samp>
                    </div>
                </div>
            </div>
        </div>
    </template>
</template>

<script lang="ts">
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import { gridProfilePresets, type GridProfilePreset } from '@/types/GridProfilePresets';
import type { GridProfileRawdata } from '@/types/GridProfileRawdata';
import type { GridProfileSection, GridProfileStatus, GridProfileValue } from '@/types/GridProfileStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { BIconExclamationTriangle, BIconInfoSquare } from 'bootstrap-icons-vue';
import { defineComponent, type PropType } from 'vue';
import StatusBadge from './StatusBadge.vue';

function toHex(x: number): string {
    return ('00' + (x & 0xff).toString(16)).slice(-2);
}

export default defineComponent({
    components: {
        BootstrapAlert,
        BIconExclamationTriangle,
        BIconInfoSquare,
        StatusBadge,
    },
    props: {
        serial: { type: String, required: true },
        gridProfileList: { type: Object as PropType<GridProfileStatus>, required: true },
        gridProfileRawList: { type: Object as PropType<GridProfileRawdata>, required: true },
    },
    data() {
        return {
            presets: gridProfilePresets as GridProfilePreset[],
            selectedPreset: '',
            editMode: false,
            workingRaw: [] as number[],
            pasteRaw: '',
            acknowledgeRisk: false,
            writeLoading: false,
            writePolling: false,
            pollTimer: null as number | null,
            showWriteAlert: false,
            writeAlertMessage: '',
            writeAlertType: 'danger',
        };
    },
    beforeUnmount() {
        this.stopPolling();
    },
    computed: {
        hasValidData(): boolean {
            const raw = this.gridProfileRawList.raw;
            if (!raw) {
                return false;
            }
            return raw.reduce((sum, x) => sum + x, 0) > 0;
        },
        presetLoaded(): boolean {
            return this.selectedPreset !== '';
        },
        selectedPresetName(): string {
            return this.presets.find((p) => p.key === this.selectedPreset)?.name ?? '';
        },
        // Structured per-value editing is only valid on the device's own profile.
        editing(): boolean {
            return this.editMode && !this.presetLoaded;
        },
        changeCount(): number {
            if (!this.editing) {
                return 0;
            }
            let count = 0;
            for (const section of this.gridProfileList.sections ?? []) {
                count += this.sectionChangeCount(section);
            }
            return count;
        },
    },
    watch: {
        // Re-initialise whenever a (new) profile is loaded into the dialog.
        gridProfileRawList: {
            handler() {
                this.resetEditable();
            },
            deep: true,
            immediate: true,
        },
    },
    methods: {
        rawContent(): string {
            const raw = this.gridProfileRawList.raw;
            if (!raw) {
                return '';
            }
            return raw.map(toHex).join(' ');
        },
        workingHex(): string {
            return this.workingRaw.map(toHex).join(' ');
        },
        resetEditable() {
            this.stopPolling();
            this.selectedPreset = '';
            this.editMode = false;
            const raw = this.gridProfileRawList.raw;
            this.workingRaw = raw ? raw.slice() : [];
            this.pasteRaw = this.workingHex();
            this.acknowledgeRisk = false;
            this.showWriteAlert = false;
        },
        onSelectPreset() {
            this.showWriteAlert = false;
            if (!this.presetLoaded) {
                // Back to the device's current profile (editable).
                const raw = this.gridProfileRawList.raw;
                this.workingRaw = raw ? raw.slice() : [];
            } else {
                const preset = this.presets.find((p) => p.key === this.selectedPreset);
                if (preset) {
                    this.workingRaw = preset.raw.slice();
                    this.editMode = false; // cannot structure-edit a foreign preset
                }
            }
            this.pasteRaw = this.workingHex();
        },
        enableEditMode() {
            if (!this.presetLoaded) {
                this.editMode = true;
            }
        },
        discardEdits() {
            const raw = this.gridProfileRawList.raw;
            this.workingRaw = raw ? raw.slice() : [];
            this.editMode = false;
            this.showWriteAlert = false;
        },
        // Decode the (signed int16) value currently held in the working buffer.
        readValue(item: GridProfileValue): number {
            const o = item.o;
            if (o === undefined || o + 1 >= this.workingRaw.length) {
                return item.v;
            }
            let iv = (((this.workingRaw[o] ?? 0) << 8) | (this.workingRaw[o + 1] ?? 0)) & 0xffff;
            if (iv > 0x7fff) {
                iv -= 0x10000;
            }
            const divider = item.d && item.d !== 0 ? item.d : 1;
            // Round to avoid floating point noise for dividers of 10 / 100.
            return Math.round((iv / divider) * 1000) / 1000;
        },
        // Encode a value back into the working buffer at the item's offset.
        writeValue(item: GridProfileValue, value: number) {
            const o = item.o;
            if (o === undefined || o + 1 >= this.workingRaw.length) {
                return;
            }
            const divider = item.d && item.d !== 0 ? item.d : 1;
            let iv = Math.round(value * divider);
            if (iv < -32768) iv = -32768;
            if (iv > 32767) iv = 32767;
            if (iv < 0) iv += 0x10000;
            this.workingRaw[o] = (iv >> 8) & 0xff;
            this.workingRaw[o + 1] = iv & 0xff;
        },
        onNumberInput(item: GridProfileValue, str: string) {
            const value = parseFloat(str);
            if (!isNaN(value)) {
                this.writeValue(item, value);
            }
        },
        stepFor(item: GridProfileValue): number | string {
            return item.d && item.d !== 0 ? 1 / item.d : 'any';
        },
        isChanged(item: GridProfileValue): boolean {
            if (item.o === undefined) {
                return false;
            }
            return Math.abs(this.readValue(item) - item.v) > 1e-9;
        },
        sectionChangeCount(section: GridProfileSection): number {
            return section.items.filter((item) => this.isChanged(item)).length;
        },
        parseRawBytes(input: string): number[] | null {
            const tokens = input
                .trim()
                .split(/[\s,]+/)
                .filter((t) => t.length > 0);
            const bytes: number[] = [];
            for (const token of tokens) {
                const value = parseInt(token, 16);
                if (isNaN(value) || value < 0 || value > 255) {
                    return null;
                }
                bytes.push(value);
            }
            return bytes;
        },
        loadPaste() {
            const bytes = this.parseRawBytes(this.pasteRaw);
            if (bytes === null || bytes.length < 6 || bytes.length > 256) {
                this.writeAlertType = 'danger';
                this.writeAlertMessage = this.$t('gridprofile.InvalidRaw');
                this.showWriteAlert = true;
                return;
            }
            this.selectedPreset = '';
            this.editMode = false;
            this.workingRaw = bytes;
            this.writeAlertType = 'info';
            this.writeAlertMessage = this.$t('gridprofile.Loaded');
            this.showWriteAlert = true;
        },
        onWriteGridProfile() {
            if (this.workingRaw.length < 6 || this.workingRaw.length > 256) {
                this.writeAlertType = 'danger';
                this.writeAlertMessage = this.$t('gridprofile.InvalidRaw');
                this.showWriteAlert = true;
                return;
            }

            const payload = {
                serial: this.serial,
                acknowledge_risk: this.acknowledgeRisk,
                raw: this.workingRaw,
            };

            const formData = new FormData();
            formData.append('data', JSON.stringify(payload));

            this.stopPolling();
            this.writeLoading = true;
            fetch('/api/gridprofile/write', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        // The POST only confirms the command was queued. The actual
                        // result (inverter acknowledged / timed out) arrives a few
                        // seconds later, so poll the status endpoint for write_status.
                        this.writeAlertType = 'info';
                        this.writeAlertMessage = this.$t('gridprofile.WritePending');
                        this.showWriteAlert = true;
                        this.startPolling();
                    } else {
                        this.writeLoading = false;
                        this.writeAlertType = response.type;
                        this.writeAlertMessage = this.$t('apiresponse.' + response.code, response.param);
                        this.showWriteAlert = true;
                    }
                })
                .catch(() => {
                    this.writeLoading = false;
                });
        },
        startPolling() {
            this.writePolling = true;
            let elapsed = 0;
            const intervalMs = 2000;
            const maxMs = 26000; // a bit beyond the ~20s inverter retry/persist window
            this.pollTimer = window.setInterval(() => {
                elapsed += intervalMs;
                fetch('/api/gridprofile/status?inv=' + this.serial, { headers: authHeader() })
                    .then((response) => handleResponse(response, this.$emitter, this.$router))
                    .then((data) => {
                        const status = data.write_status;
                        if (status === 'Ok') {
                            this.finishWrite('success', 'gridprofile.WriteSuccess');
                        } else if (status === 'Failure') {
                            this.finishWrite('danger', 'gridprofile.WriteFailure');
                        } else if (elapsed >= maxMs) {
                            this.finishWrite('warning', 'gridprofile.WriteTimeout');
                        }
                        // status === 'Pending' (or anything else): keep waiting
                    })
                    .catch(() => {
                        // Ignore transient polling errors; the maxMs guard ends it.
                        if (elapsed >= maxMs) {
                            this.finishWrite('warning', 'gridprofile.WriteTimeout');
                        }
                    });
            }, intervalMs);
        },
        finishWrite(type: string, messageKey: string) {
            this.stopPolling();
            this.writeLoading = false;
            this.writeAlertType = type;
            this.writeAlertMessage = this.$t(messageKey);
            this.showWriteAlert = true;
        },
        stopPolling() {
            if (this.pollTimer !== null) {
                window.clearInterval(this.pollTimer);
                this.pollTimer = null;
            }
            this.writePolling = false;
        },
    },
});
</script>

<style scoped>
.gridprofile-value {
    max-width: 220px;
}
</style>
