export interface GridProfileValue {
    n: string;
    u: string;
    v: number;
    o?: number; // byte offset of the int16 value within the raw profile payload
    d?: number; // divider: value == int16(raw) / d
}

export interface GridProfileSection {
    name: string;
    items: Array<GridProfileValue>;
}

export interface GridProfileStatus {
    name: string;
    version: string;
    sections: Array<GridProfileSection>;
}
